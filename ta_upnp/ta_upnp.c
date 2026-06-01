/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Agent-side UPnP control point over libupnp (pupnp)
 *
 * Written against the pupnp 1.14 API (the @c UpnpDiscovery_get_*_cstr
 * accessors and @c UpnpString). Discovery is asynchronous in libupnp:
 * UpnpSearchAsync() returns at once and the responses arrive on the
 * SDK's own thread through the client callback, so ta_upnp_discover()
 * collects for @a mx seconds under a mutex and then reads the result.
 * The raw SSDP probe does not use libupnp at all - it is a UDP socket,
 * so the bytes it counts are the real ones on the wire.
 */

#define TE_LGR_USER     "TA UPnP"

#include "te_config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <upnp/upnp.h>
#include <upnp/upnptools.h>
#include <upnp/ixml.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"
#include "logger_api.h"

#include "ta_upnp.h"

/** The one control-point handle, and whether it is up. */
static UpnpClient_Handle ta_upnp_handle;
static bool ta_upnp_up = false;

/** Discovery results accumulate here between UpnpSearchAsync and the wait. */
static pthread_mutex_t ta_upnp_lock = PTHREAD_MUTEX_INITIALIZER;
static te_string ta_upnp_results = TE_STRING_INIT;
static int ta_upnp_count;

/** A libupnp return code turned into a TE status, with the message logged. */
static te_errno
upnp_rc(int r, const char *what)
{
    if (r == UPNP_E_SUCCESS)
        return 0;

    ERROR("%s: libupnp %d (%s)", what, r, UpnpGetErrorMessage(r));
    switch (r)
    {
        case UPNP_E_INVALID_PARAM:
        case UPNP_E_INVALID_URL:
            return TE_RC(TE_TA_UNIX, TE_EINVAL);
        case UPNP_E_OUTOF_MEMORY:
            return TE_RC(TE_TA_UNIX, TE_ENOMEM);
        case UPNP_E_TIMEDOUT:
            return TE_RC(TE_TA_UNIX, TE_ETIMEDOUT);
        case UPNP_E_SOCKET_CONNECT:
        case UPNP_E_SOCKET_ERROR:
            return TE_RC(TE_TA_UNIX, TE_ECONNREFUSED);
        default:
            return TE_RC(TE_TA_UNIX, TE_EFAIL);
    }
}

/** The client callback: gather SSDP search results; ignore the rest. */
static int
ta_upnp_cb(Upnp_EventType type, const void *event, void *cookie)
{
    UNUSED(cookie);

    if (type == UPNP_DISCOVERY_SEARCH_RESULT)
    {
        const UpnpDiscovery *d = event;
        const char *usn = UpnpDiscovery_get_DeviceID_cstr(d);
        const char *svc = UpnpDiscovery_get_ServiceType_cstr(d);
        const char *dev = UpnpDiscovery_get_DeviceType_cstr(d);
        const char *loc = UpnpDiscovery_get_Location_cstr(d);
        const char *os = UpnpDiscovery_get_Os_cstr(d);
        const char *st = (svc != NULL && *svc != '\0') ? svc : dev;

        pthread_mutex_lock(&ta_upnp_lock);
        te_string_append(&ta_upnp_results, "%s\t%s\t%s\t%s\n",
                         usn != NULL ? usn : "",
                         st != NULL ? st : "",
                         loc != NULL ? loc : "",
                         os != NULL ? os : "");
        ta_upnp_count++;
        pthread_mutex_unlock(&ta_upnp_lock);
    }

    return 0;
}

/* See description in ta_upnp.h */
te_errno
ta_upnp_init(const char *iface, int port)
{
    int r;

    if (ta_upnp_up)
        return 0;

    r = UpnpInit2(iface != NULL && *iface != '\0' ? iface : NULL,
                  (unsigned short)port);
    if (r != UPNP_E_SUCCESS)
        return upnp_rc(r, "UpnpInit2");

    r = UpnpRegisterClient(ta_upnp_cb, NULL, &ta_upnp_handle);
    if (r != UPNP_E_SUCCESS)
    {
        UpnpFinish();
        return upnp_rc(r, "UpnpRegisterClient");
    }

    ta_upnp_up = true;
    return 0;
}

/* See description in ta_upnp.h */
te_errno
ta_upnp_finish(void)
{
    if (ta_upnp_up)
    {
        UpnpUnRegisterClient(ta_upnp_handle);
        UpnpFinish();
        ta_upnp_up = false;
    }
    pthread_mutex_lock(&ta_upnp_lock);
    te_string_free(&ta_upnp_results);
    pthread_mutex_unlock(&ta_upnp_lock);

    return 0;
}

/* See description in ta_upnp.h */
te_errno
ta_upnp_discover(const char *st, int mx, int *count, te_string *result)
{
    int r;

    if (!ta_upnp_up)
        return TE_RC(TE_TA_UNIX, TE_EFAIL);
    if (mx <= 0)
        mx = 3;

    pthread_mutex_lock(&ta_upnp_lock);
    te_string_reset(&ta_upnp_results);
    ta_upnp_count = 0;
    pthread_mutex_unlock(&ta_upnp_lock);

    r = UpnpSearchAsync(ta_upnp_handle, mx,
                        st != NULL ? st : "ssdp:all", NULL);
    if (r != UPNP_E_SUCCESS)
        return upnp_rc(r, "UpnpSearchAsync");

    /* Answers arrive on the SDK thread; give them the MX window and one more. */
    sleep((unsigned int)mx + 1);

    pthread_mutex_lock(&ta_upnp_lock);
    if (result != NULL)
        te_string_append(result, "%s", te_string_value(&ta_upnp_results));
    if (count != NULL)
        *count = ta_upnp_count;
    pthread_mutex_unlock(&ta_upnp_lock);

    return 0;
}

/* See description in ta_upnp.h */
te_errno
ta_upnp_describe(const char *location, te_string *xml)
{
    IXML_Document *doc = NULL;
    char *text;
    int r;

    if (location == NULL)
        return TE_RC(TE_TA_UNIX, TE_EINVAL);

    r = UpnpDownloadXmlDoc(location, &doc);
    if (r != UPNP_E_SUCCESS)
        return upnp_rc(r, "UpnpDownloadXmlDoc");

    text = ixmlDocumenttoString(doc);
    if (text != NULL)
    {
        if (xml != NULL)
            te_string_append(xml, "%s", text);
        ixmlFreeDOMString(text);
    }
    ixmlDocument_free(doc);

    return text != NULL ? 0 : TE_RC(TE_TA_UNIX, TE_ENOMEM);
}

/* See description in ta_upnp.h */
te_errno
ta_upnp_get(const char *url, te_string *body)
{
    char *buf = NULL;
    char content_type[LINE_SIZE] = "";
    int r;

    if (url == NULL)
        return TE_RC(TE_TA_UNIX, TE_EINVAL);

    r = UpnpDownloadUrlItem(url, &buf, content_type);
    if (r != UPNP_E_SUCCESS)
        return upnp_rc(r, "UpnpDownloadUrlItem");

    if (buf != NULL)
    {
        if (body != NULL)
            te_string_append(body, "%s", buf);
        free(buf);
    }

    return 0;
}

/** Build a libupnp action document from "name=value" lines. */
static te_errno
upnp_build_action(const char *action, const char *service_type,
                  const char *args, IXML_Document **out)
{
    IXML_Document *doc = NULL;
    const char *line = args;

    while (line != NULL && *line != '\0')
    {
        const char *nl = strchr(line, '\n');
        size_t len = nl != NULL ? (size_t)(nl - line) : strlen(line);
        const char *eq = memchr(line, '=', len);

        if (eq != NULL)
        {
            char name[256];
            char *value = TE_ALLOC((size_t)(len - (eq - line)) + 1);
            size_t nlen = (size_t)(eq - line);

            if (nlen >= sizeof(name))
                nlen = sizeof(name) - 1;
            memcpy(name, line, nlen);
            name[nlen] = '\0';
            memcpy(value, eq + 1, len - (size_t)(eq - line) - 1);
            value[len - (size_t)(eq - line) - 1] = '\0';

            if (UpnpAddToAction(&doc, action, service_type, name,
                                value) != UPNP_E_SUCCESS)
            {
                free(value);
                if (doc != NULL)
                    ixmlDocument_free(doc);
                return TE_RC(TE_TA_UNIX, TE_EFAIL);
            }
            free(value);
        }

        line = nl != NULL ? nl + 1 : NULL;
    }

    if (doc == NULL)
        doc = UpnpMakeAction(action, service_type, 0, NULL);
    if (doc == NULL)
        return TE_RC(TE_TA_UNIX, TE_EFAIL);

    *out = doc;
    return 0;
}

/* See description in ta_upnp.h */
te_errno
ta_upnp_action(const char *control_url, const char *service_type,
               const char *action, const char *args, te_string *response,
               te_string *fault)
{
    IXML_Document *action_doc = NULL;
    IXML_Document *resp = NULL;
    char *text;
    te_errno rc;
    int r;

    if (!ta_upnp_up)
        return TE_RC(TE_TA_UNIX, TE_EFAIL);
    if (control_url == NULL || service_type == NULL || action == NULL)
        return TE_RC(TE_TA_UNIX, TE_EINVAL);

    rc = upnp_build_action(action, service_type, args, &action_doc);
    if (rc != 0)
        return rc;

    r = UpnpSendAction(ta_upnp_handle, control_url, service_type, NULL,
                       action_doc, &resp);
    ixmlDocument_free(action_doc);

    /*
     * A SOAP fault comes back as a non-success code with the fault in
     * resp; its errorCode/errorDescription are the useful part. A
     * success gives the response body.
     */
    if (resp != NULL)
    {
        text = ixmlDocumenttoString(resp);
        if (text != NULL)
        {
            if (r == UPNP_E_SUCCESS)
            {
                if (response != NULL)
                    te_string_append(response, "%s", text);
            }
            else if (fault != NULL)
            {
                te_string_append(fault, "%s", text);
            }
            ixmlFreeDOMString(text);
        }
        ixmlDocument_free(resp);
    }

    return r == UPNP_E_SUCCESS ? 0 : upnp_rc(r, "UpnpSendAction");
}

/* See description in ta_upnp.h */
te_errno
ta_upnp_subscribe(const char *event_url, int timeout, te_string *sid,
                  int *actual_timeout)
{
    Upnp_SID sid_buf;
    int t = timeout > 0 ? timeout : -1; /* libupnp: -1 is infinite. */
    int r;

    if (!ta_upnp_up)
        return TE_RC(TE_TA_UNIX, TE_EFAIL);
    if (event_url == NULL)
        return TE_RC(TE_TA_UNIX, TE_EINVAL);

    memset(sid_buf, 0, sizeof(sid_buf));
    r = UpnpSubscribe(ta_upnp_handle, event_url, &t, sid_buf);
    if (r != UPNP_E_SUCCESS)
        return upnp_rc(r, "UpnpSubscribe");

    if (sid != NULL)
        te_string_append(sid, "%s", sid_buf);
    if (actual_timeout != NULL)
        *actual_timeout = t;

    return 0;
}

/* See description in ta_upnp.h */
te_errno
ta_upnp_renew(const char *sid, int timeout, int *actual_timeout)
{
    Upnp_SID sid_buf;
    int t = timeout > 0 ? timeout : -1;
    int r;

    if (!ta_upnp_up || sid == NULL)
        return TE_RC(TE_TA_UNIX, TE_EINVAL);

    te_strlcpy(sid_buf, sid, sizeof(sid_buf));
    r = UpnpRenewSubscription(ta_upnp_handle, &t, sid_buf);
    if (r != UPNP_E_SUCCESS)
        return upnp_rc(r, "UpnpRenewSubscription");

    if (actual_timeout != NULL)
        *actual_timeout = t;

    return 0;
}

/* See description in ta_upnp.h */
te_errno
ta_upnp_unsubscribe(const char *sid)
{
    Upnp_SID sid_buf;
    int r;

    if (!ta_upnp_up || sid == NULL)
        return TE_RC(TE_TA_UNIX, TE_EINVAL);

    te_strlcpy(sid_buf, sid, sizeof(sid_buf));
    r = UpnpUnSubscribe(ta_upnp_handle, sid_buf);
    if (r != UPNP_E_SUCCESS)
        return upnp_rc(r, "UpnpUnSubscribe");

    return 0;
}

/** The SSDP multicast group and port. */
#define TA_UPNP_SSDP_ADDR "239.255.255.250"
#define TA_UPNP_SSDP_PORT 1900

/* See description in ta_upnp.h */
te_errno
ta_upnp_ssdp_probe(const char *st, int mx, int *responders,
                   unsigned int *bytes_out, unsigned int *bytes_in,
                   te_string *detail)
{
    te_string request = TE_STRING_INIT;
    te_string seen = TE_STRING_INIT;
    struct sockaddr_in dst;
    struct timeval tv;
    int count = 0;
    unsigned int in_total = 0;
    int sock;
    ssize_t n;
    te_errno rc = 0;

    if (mx <= 0)
        mx = 2;

    te_string_append(&request,
                     "M-SEARCH * HTTP/1.1\r\n"
                     "HOST: %s:%d\r\n"
                     "MAN: \"ssdp:discover\"\r\n"
                     "MX: %d\r\n"
                     "ST: %s\r\n\r\n",
                     TA_UPNP_SSDP_ADDR, TA_UPNP_SSDP_PORT, mx,
                     st != NULL && *st != '\0' ? st : "ssdp:all");

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        rc = TE_OS_RC(TE_TA_UNIX, errno);
        goto out;
    }

    tv.tv_sec = mx + 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port = htons(TA_UPNP_SSDP_PORT);
    dst.sin_addr.s_addr = inet_addr(TA_UPNP_SSDP_ADDR);

    n = sendto(sock, request.ptr, request.len, 0,
               (struct sockaddr *)&dst, sizeof(dst));
    if (n < 0)
    {
        rc = TE_OS_RC(TE_TA_UNIX, errno);
        close(sock);
        goto out;
    }
    if (bytes_out != NULL)
        *bytes_out = (unsigned int)request.len;

    for (;;)
    {
        char buf[2048];
        struct sockaddr_in from;
        socklen_t flen = sizeof(from);
        char addr[64];

        n = recvfrom(sock, buf, sizeof(buf), 0,
                     (struct sockaddr *)&from, &flen);
        if (n <= 0)
            break; /* timeout or error ends the collection. */

        in_total += (unsigned int)n;
        snprintf(addr, sizeof(addr), "%s:%d", inet_ntoa(from.sin_addr),
                 ntohs(from.sin_port));

        /* Count each responder once; tally its bytes. */
        if (strstr(te_string_value(&seen), addr) == NULL)
        {
            te_string_append(&seen, "%s;", addr);
            count++;
        }
        if (detail != NULL)
            te_string_append(detail, "%s\t%zd\n", addr, (ssize_t)n);
    }

    close(sock);

    if (responders != NULL)
        *responders = count;
    if (bytes_in != NULL)
        *bytes_in = in_total;

out:
    te_string_free(&request);
    te_string_free(&seen);

    return rc;
}
