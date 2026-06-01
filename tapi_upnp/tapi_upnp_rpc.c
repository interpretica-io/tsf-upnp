/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief UPnP TAPI: RPC client wrappers
 *
 * The rcf_rpc_call() boilerplate behind tapi_upnp. The RPCs return
 * te_errno; an RPC transport failure is mapped to TE_ECORRUPTED.
 */

#define TE_LGR_USER     "TAPI UPnP RPC"

#include "te_config.h"

#include <string.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_string.h"
#include "logger_api.h"
#include "tapi_rpc_internal.h"
#include "tarpc.h"

#include "tapi_upnp_rpc.h"

#define CHECK_RPC_ERRNO_UNCHANGED(_func, _var) \
    CHECK_RETVAL_VAR_ERR_COND(_func, _var, false,                    \
                              TE_RC(TE_TAPI, TE_ECORRUPTED), false)

/* Append an RPC string result, when there is one. */
static void
take_string(te_string *dst, const char *src)
{
    if (dst != NULL && src != NULL)
        te_string_append(dst, "%s", src);
}

/* See description in tapi_upnp_rpc.h */
te_errno
rpc_upnp_init(rcf_rpc_server *rpcs, const char *iface, int port)
{
    tarpc_upnp_init_in in;
    tarpc_upnp_init_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.iface = (char *)iface;
    in.port = port;

    rcf_rpc_call(rpcs, "upnp_init", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(upnp_init, out.retval);
    TAPI_RPC_LOG(rpcs, upnp_init, "%s, %d", "%r",
                 iface != NULL ? iface : "", port, out.retval);
    RETVAL_TE_ERRNO(upnp_init, out.retval);
}

/* See description in tapi_upnp_rpc.h */
te_errno
rpc_upnp_finish(rcf_rpc_server *rpcs)
{
    tarpc_upnp_finish_in in;
    tarpc_upnp_finish_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    rcf_rpc_call(rpcs, "upnp_finish", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(upnp_finish, out.retval);
    TAPI_RPC_LOG(rpcs, upnp_finish, "", "%r", out.retval);
    RETVAL_TE_ERRNO(upnp_finish, out.retval);
}

/* See description in tapi_upnp_rpc.h */
te_errno
rpc_upnp_discover(rcf_rpc_server *rpcs, const char *st, int mx, int *count,
                  te_string *result)
{
    tarpc_upnp_discover_in in;
    tarpc_upnp_discover_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.st = (char *)st;
    in.mx = mx;

    rcf_rpc_call(rpcs, "upnp_discover", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(upnp_discover, out.retval);
    TAPI_RPC_LOG(rpcs, upnp_discover, "%s, mx=%d", "%r count=%d",
                 st != NULL ? st : "", mx, out.retval, out.count);

    if (out.retval == 0)
    {
        if (count != NULL)
            *count = out.count;
        take_string(result, out.result);
    }
    RETVAL_TE_ERRNO(upnp_discover, out.retval);
}

/* See description in tapi_upnp_rpc.h */
te_errno
rpc_upnp_describe(rcf_rpc_server *rpcs, const char *location, te_string *xml)
{
    tarpc_upnp_describe_in in;
    tarpc_upnp_describe_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.location = (char *)location;

    rcf_rpc_call(rpcs, "upnp_describe", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(upnp_describe, out.retval);
    TAPI_RPC_LOG(rpcs, upnp_describe, "%s", "%r",
                 location != NULL ? location : "", out.retval);

    if (out.retval == 0)
        take_string(xml, out.xml);
    RETVAL_TE_ERRNO(upnp_describe, out.retval);
}

/* See description in tapi_upnp_rpc.h */
te_errno
rpc_upnp_get(rcf_rpc_server *rpcs, const char *url, te_string *body)
{
    tarpc_upnp_get_in in;
    tarpc_upnp_get_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.url = (char *)url;

    rcf_rpc_call(rpcs, "upnp_get", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(upnp_get, out.retval);
    TAPI_RPC_LOG(rpcs, upnp_get, "%s", "%r", url != NULL ? url : "",
                 out.retval);

    if (out.retval == 0)
        take_string(body, out.body);
    RETVAL_TE_ERRNO(upnp_get, out.retval);
}

/* See description in tapi_upnp_rpc.h */
te_errno
rpc_upnp_action(rcf_rpc_server *rpcs, const char *control_url,
                const char *service_type, const char *action,
                const char *args, te_string *response, te_string *fault)
{
    tarpc_upnp_action_in in;
    tarpc_upnp_action_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.control_url = (char *)control_url;
    in.service_type = (char *)service_type;
    in.action = (char *)action;
    in.args = (char *)(args != NULL ? args : "");

    rcf_rpc_call(rpcs, "upnp_action", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(upnp_action, out.retval);
    TAPI_RPC_LOG(rpcs, upnp_action, "%s %s", "%r",
                 action != NULL ? action : "",
                 service_type != NULL ? service_type : "", out.retval);

    take_string(response, out.response);
    take_string(fault, out.fault);
    RETVAL_TE_ERRNO(upnp_action, out.retval);
}

/* See description in tapi_upnp_rpc.h */
te_errno
rpc_upnp_subscribe(rcf_rpc_server *rpcs, const char *event_url, int timeout,
                   te_string *sid, int *actual_timeout)
{
    tarpc_upnp_subscribe_in in;
    tarpc_upnp_subscribe_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.event_url = (char *)event_url;
    in.timeout = timeout;

    rcf_rpc_call(rpcs, "upnp_subscribe", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(upnp_subscribe, out.retval);
    TAPI_RPC_LOG(rpcs, upnp_subscribe, "%s", "%r sid=%s",
                 event_url != NULL ? event_url : "", out.retval,
                 out.sid != NULL ? out.sid : "");

    if (out.retval == 0)
    {
        take_string(sid, out.sid);
        if (actual_timeout != NULL)
            *actual_timeout = out.actual_timeout;
    }
    RETVAL_TE_ERRNO(upnp_subscribe, out.retval);
}

/* See description in tapi_upnp_rpc.h */
te_errno
rpc_upnp_renew(rcf_rpc_server *rpcs, const char *sid, int timeout,
               int *actual_timeout)
{
    tarpc_upnp_renew_in in;
    tarpc_upnp_renew_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.sid = (char *)sid;
    in.timeout = timeout;

    rcf_rpc_call(rpcs, "upnp_renew", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(upnp_renew, out.retval);
    TAPI_RPC_LOG(rpcs, upnp_renew, "%s", "%r",
                 sid != NULL ? sid : "", out.retval);

    if (out.retval == 0 && actual_timeout != NULL)
        *actual_timeout = out.actual_timeout;
    RETVAL_TE_ERRNO(upnp_renew, out.retval);
}

/* See description in tapi_upnp_rpc.h */
te_errno
rpc_upnp_unsubscribe(rcf_rpc_server *rpcs, const char *sid)
{
    tarpc_upnp_unsubscribe_in in;
    tarpc_upnp_unsubscribe_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.sid = (char *)sid;

    rcf_rpc_call(rpcs, "upnp_unsubscribe", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(upnp_unsubscribe, out.retval);
    TAPI_RPC_LOG(rpcs, upnp_unsubscribe, "%s", "%r",
                 sid != NULL ? sid : "", out.retval);
    RETVAL_TE_ERRNO(upnp_unsubscribe, out.retval);
}

/* See description in tapi_upnp_rpc.h */
te_errno
rpc_upnp_ssdp_probe(rcf_rpc_server *rpcs, const char *st, int mx,
                    int *responders, unsigned int *bytes_out,
                    unsigned int *bytes_in, te_string *detail)
{
    tarpc_upnp_ssdp_probe_in in;
    tarpc_upnp_ssdp_probe_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.st = (char *)st;
    in.mx = mx;

    rcf_rpc_call(rpcs, "upnp_ssdp_probe", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(upnp_ssdp_probe, out.retval);
    TAPI_RPC_LOG(rpcs, upnp_ssdp_probe, "%s", "%r responders=%d in=%u out=%u",
                 st != NULL ? st : "", out.retval, out.responders,
                 out.bytes_in, out.bytes_out);

    if (out.retval == 0)
    {
        if (responders != NULL)
            *responders = out.responders;
        if (bytes_out != NULL)
            *bytes_out = out.bytes_out;
        if (bytes_in != NULL)
            *bytes_in = out.bytes_in;
        take_string(detail, out.detail);
    }
    RETVAL_TE_ERRNO(upnp_ssdp_probe, out.retval);
}
