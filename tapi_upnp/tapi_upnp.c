/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief UPnP TAPI
 *
 * The engine-side face of the upnp_* RPCs. The RPCs deal in text; the
 * list splitting and the small XML reads are done here, so a test gets
 * #tapi_upnp_device, #tapi_upnp_service and argument values. The XML
 * reader is deliberately small and namespace-agnostic - it finds a
 * @c <tag>value</tag>, which is all a UPnP description or a SOAP
 * response body needs - not a general parser.
 */

#define TE_LGR_USER     "TAPI UPnP"

#include "te_config.h"

#include <string.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"
#include "te_vector.h"
#include "logger_api.h"

#include "tapi_upnp.h"
#include "tapi_upnp_rpc.h"

/* See description in tapi_upnp.h */
te_errno
tapi_upnp_start(rcf_rpc_server *rpcs, const char *iface)
{
    return rpc_upnp_init(rpcs, iface, 0);
}

/* See description in tapi_upnp.h */
te_errno
tapi_upnp_stop(rcf_rpc_server *rpcs)
{
    return rpc_upnp_finish(rpcs);
}

/** Copy up to @p n bytes of @p src into a fresh string. */
static char *
dup_n(const char *src, size_t n)
{
    char *s = TE_ALLOC(n + 1);

    memcpy(s, src, n);
    s[n] = '\0';
    return s;
}

/* See description in tapi_upnp.h */
te_errno
tapi_upnp_discover(rcf_rpc_server *rpcs, const char *st, int mx,
                   te_vec *devices)
{
    te_string result = TE_STRING_INIT;
    const char *line;
    int count = 0;
    te_errno rc;

    rc = rpc_upnp_discover(rpcs, st, mx, &count, &result);
    if (rc != 0)
    {
        te_string_free(&result);
        return rc;
    }

    for (line = te_string_value(&result); line != NULL && *line != '\0'; )
    {
        const char *nl = strchr(line, '\n');
        size_t len = nl != NULL ? (size_t)(nl - line) : strlen(line);
        const char *fields[4] = { "", "", "", "" };
        char *copy;
        char *p;
        int i = 0;

        if (len != 0)
        {
            tapi_upnp_device dev;

            copy = dup_n(line, len);
            /* Split the line on tabs into up to four fields. */
            for (p = copy, fields[0] = copy; *p != '\0' && i < 3; p++)
            {
                if (*p == '\t')
                {
                    *p = '\0';
                    fields[++i] = p + 1;
                }
            }

            dev.usn = TE_STRDUP(fields[0]);
            dev.st = TE_STRDUP(fields[1]);
            dev.location = TE_STRDUP(fields[2]);
            dev.server = TE_STRDUP(fields[3]);
            TE_VEC_APPEND(devices, dev);
            free(copy);
        }

        line = nl != NULL ? nl + 1 : NULL;
    }

    te_string_free(&result);
    return 0;
}

/* See description in tapi_upnp.h */
void
tapi_upnp_devices_free(te_vec *devices)
{
    tapi_upnp_device *dev;

    TE_VEC_FOREACH(devices, dev)
    {
        free(dev->usn);
        free(dev->st);
        free(dev->location);
        free(dev->server);
    }
    te_vec_free(devices);
}

/* See description in tapi_upnp.h */
te_errno
tapi_upnp_describe(rcf_rpc_server *rpcs, const char *location, te_string *xml)
{
    return rpc_upnp_describe(rpcs, location, xml);
}

/** Find the content of the first <tag>...</tag> in @p xml. */
static bool
xml_field_in(const char *xml, const char *tag, te_string *value)
{
    te_string open = TE_STRING_INIT;
    te_string close = TE_STRING_INIT;
    const char *start;
    const char *end;
    bool found = false;

    te_string_append(&open, "<%s>", tag);
    te_string_append(&close, "</%s>", tag);

    start = strstr(xml, open.ptr);
    if (start != NULL)
    {
        start += open.len;
        end = strstr(start, close.ptr);
        if (end != NULL)
        {
            if (value != NULL)
                te_string_append(value, "%.*s", (int)(end - start), start);
            found = true;
        }
    }

    te_string_free(&open);
    te_string_free(&close);
    return found;
}

/* See description in tapi_upnp.h */
bool
tapi_upnp_xml_field(const char *xml, const char *tag, te_string *value)
{
    if (xml == NULL || tag == NULL)
        return false;
    return xml_field_in(xml, tag, value);
}

/** The scheme://authority of @p url, i.e. up to the third slash. */
static void
url_origin(const char *url, te_string *origin)
{
    const char *p = strstr(url, "://");

    if (p == NULL)
        return;
    p += 3;
    p += strcspn(p, "/");
    te_string_append(origin, "%.*s", (int)(p - url), url);
}

/** Resolve @p ref against @p base into @p out (absolute, /path or path). */
static void
url_resolve(const char *base, const char *ref, te_string *out)
{
    if (ref == NULL || *ref == '\0')
        return;

    if (strncmp(ref, "http://", 7) == 0 || strncmp(ref, "https://", 8) == 0)
    {
        te_string_append(out, "%s", ref);
        return;
    }

    url_origin(base, out);
    if (*ref != '/')
        te_string_append(out, "/");
    te_string_append(out, "%s", ref);
}

/* See description in tapi_upnp.h */
te_errno
tapi_upnp_services(const char *xml, const char *base_url, te_vec *services)
{
    const char *pos;

    if (xml == NULL || services == NULL)
        return TE_RC(TE_TAPI, TE_EINVAL);

    for (pos = xml; (pos = strstr(pos, "<service>")) != NULL; )
    {
        const char *end = strstr(pos, "</service>");
        char *block;
        tapi_upnp_service svc;
        te_string ctl = TE_STRING_INIT;
        te_string evt = TE_STRING_INIT;
        te_string scpd = TE_STRING_INIT;
        te_string tmp = TE_STRING_INIT;

        if (end == NULL)
            break;
        block = dup_n(pos, (size_t)(end - pos));

        memset(&svc, 0, sizeof(svc));

        if (xml_field_in(block, "serviceType", &tmp))
            svc.service_type = TE_STRDUP(tmp.ptr);
        te_string_reset(&tmp);
        if (xml_field_in(block, "serviceId", &tmp))
            svc.service_id = TE_STRDUP(tmp.ptr);
        te_string_reset(&tmp);

        if (xml_field_in(block, "controlURL", &tmp))
            url_resolve(base_url != NULL ? base_url : "", tmp.ptr, &ctl);
        te_string_reset(&tmp);
        if (xml_field_in(block, "eventSubURL", &tmp))
            url_resolve(base_url != NULL ? base_url : "", tmp.ptr, &evt);
        te_string_reset(&tmp);
        if (xml_field_in(block, "SCPDURL", &tmp))
            url_resolve(base_url != NULL ? base_url : "", tmp.ptr, &scpd);

        svc.control_url = TE_STRDUP(ctl.ptr != NULL ? ctl.ptr : "");
        svc.event_sub_url = TE_STRDUP(evt.ptr != NULL ? evt.ptr : "");
        svc.scpd_url = TE_STRDUP(scpd.ptr != NULL ? scpd.ptr : "");
        if (svc.service_type == NULL)
            svc.service_type = TE_STRDUP("");
        if (svc.service_id == NULL)
            svc.service_id = TE_STRDUP("");

        TE_VEC_APPEND(services, svc);

        free(block);
        te_string_free(&ctl);
        te_string_free(&evt);
        te_string_free(&scpd);
        te_string_free(&tmp);

        pos = end + strlen("</service>");
    }

    return 0;
}

/* See description in tapi_upnp.h */
void
tapi_upnp_services_free(te_vec *services)
{
    tapi_upnp_service *svc;

    TE_VEC_FOREACH(services, svc)
    {
        free(svc->service_type);
        free(svc->service_id);
        free(svc->control_url);
        free(svc->event_sub_url);
        free(svc->scpd_url);
    }
    te_vec_free(services);
}

/* See description in tapi_upnp.h */
te_errno
tapi_upnp_action(rcf_rpc_server *rpcs, const char *control_url,
                 const char *service_type, const char *action,
                 const char *args, te_string *response, te_string *fault)
{
    return rpc_upnp_action(rpcs, control_url, service_type, action, args,
                           response, fault);
}

/* See description in tapi_upnp.h */
bool
tapi_upnp_out_arg(const char *response, const char *name, te_string *value)
{
    if (response == NULL || name == NULL)
        return false;
    return xml_field_in(response, name, value);
}

/* See description in tapi_upnp.h */
te_errno
tapi_upnp_subscribe(rcf_rpc_server *rpcs, const char *event_url, int timeout,
                    te_string *sid, int *actual_timeout)
{
    return rpc_upnp_subscribe(rpcs, event_url, timeout, sid, actual_timeout);
}

/* See description in tapi_upnp.h */
te_errno
tapi_upnp_renew(rcf_rpc_server *rpcs, const char *sid, int timeout,
                int *actual_timeout)
{
    return rpc_upnp_renew(rpcs, sid, timeout, actual_timeout);
}

/* See description in tapi_upnp.h */
te_errno
tapi_upnp_unsubscribe(rcf_rpc_server *rpcs, const char *sid)
{
    return rpc_upnp_unsubscribe(rpcs, sid);
}

/* See description in tapi_upnp.h */
te_errno
tapi_upnp_ssdp_probe(rcf_rpc_server *rpcs, const char *st, int mx,
                     int *responders, unsigned int *bytes_out,
                     unsigned int *bytes_in, te_string *detail)
{
    return rpc_upnp_ssdp_probe(rpcs, st, mx, responders, bytes_out,
                               bytes_in, detail);
}
