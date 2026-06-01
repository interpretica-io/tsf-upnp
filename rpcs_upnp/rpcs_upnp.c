/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief UPnP RPC server library
 *
 * The upnp_* RPCs (see upnp_rpc.x.m4) on top of ta_upnp.
 * TARPC_FUNC_STATIC() binds an RPC to the function of the same name,
 * so each RPC has a plain C function first and the wrapper after it.
 */

#define TE_LGR_USER     "RPC UPnP"

#include "te_config.h"

#include <string.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"
#include "rpc_server.h"

#include "ta_upnp.h"

/* Hand a te_string result over to an RPC string field. */
static char *
take(te_string *str)
{
    return str->ptr != NULL ? str->ptr : TE_STRDUP("");
}

static te_errno
upnp_init(const char *iface, int port)
{
    return ta_upnp_init(iface, port);
}

TARPC_FUNC_STATIC(upnp_init, {},
{
    MAKE_CALL(out->retval = func(in->iface, in->port));
    out->common.errno_changed = false;
})

static te_errno
upnp_finish(void)
{
    return ta_upnp_finish();
}

TARPC_FUNC_STATIC(upnp_finish, {},
{
    MAKE_CALL(out->retval = func());
    out->common.errno_changed = false;
})

static te_errno
upnp_discover(const char *st, int mx, int *count, char **result)
{
    te_string r = TE_STRING_INIT;
    te_errno rc = ta_upnp_discover(st, mx, count, &r);

    *result = take(&r);
    return rc;
}

TARPC_FUNC_STATIC(upnp_discover, {},
{
    int count = 0;

    MAKE_CALL(out->retval = func(in->st, in->mx, &count, &out->result));
    out->count = count;
    out->common.errno_changed = false;
})

static te_errno
upnp_describe(const char *location, char **xml)
{
    te_string x = TE_STRING_INIT;
    te_errno rc = ta_upnp_describe(location, &x);

    *xml = take(&x);
    return rc;
}

TARPC_FUNC_STATIC(upnp_describe, {},
{
    MAKE_CALL(out->retval = func(in->location, &out->xml));
    out->common.errno_changed = false;
})

static te_errno
upnp_get(const char *url, char **body)
{
    te_string b = TE_STRING_INIT;
    te_errno rc = ta_upnp_get(url, &b);

    *body = take(&b);
    return rc;
}

TARPC_FUNC_STATIC(upnp_get, {},
{
    MAKE_CALL(out->retval = func(in->url, &out->body));
    out->common.errno_changed = false;
})

static te_errno
upnp_action(const char *control_url, const char *service_type,
            const char *action, const char *args, char **response,
            char **fault)
{
    te_string resp = TE_STRING_INIT;
    te_string flt = TE_STRING_INIT;
    te_errno rc = ta_upnp_action(control_url, service_type, action, args,
                                 &resp, &flt);

    *response = take(&resp);
    *fault = take(&flt);
    return rc;
}

TARPC_FUNC_STATIC(upnp_action, {},
{
    MAKE_CALL(out->retval = func(in->control_url, in->service_type,
                                 in->action, in->args, &out->response,
                                 &out->fault));
    out->common.errno_changed = false;
})

static te_errno
upnp_subscribe(const char *event_url, int timeout, char **sid,
               int *actual_timeout)
{
    te_string s = TE_STRING_INIT;
    te_errno rc = ta_upnp_subscribe(event_url, timeout, &s, actual_timeout);

    *sid = take(&s);
    return rc;
}

TARPC_FUNC_STATIC(upnp_subscribe, {},
{
    int actual = 0;

    MAKE_CALL(out->retval = func(in->event_url, in->timeout, &out->sid,
                                 &actual));
    out->actual_timeout = actual;
    out->common.errno_changed = false;
})

static te_errno
upnp_renew(const char *sid, int timeout, int *actual_timeout)
{
    return ta_upnp_renew(sid, timeout, actual_timeout);
}

TARPC_FUNC_STATIC(upnp_renew, {},
{
    int actual = 0;

    MAKE_CALL(out->retval = func(in->sid, in->timeout, &actual));
    out->actual_timeout = actual;
    out->common.errno_changed = false;
})

static te_errno
upnp_unsubscribe(const char *sid)
{
    return ta_upnp_unsubscribe(sid);
}

TARPC_FUNC_STATIC(upnp_unsubscribe, {},
{
    MAKE_CALL(out->retval = func(in->sid));
    out->common.errno_changed = false;
})

static te_errno
upnp_ssdp_probe(const char *st, int mx, int *responders,
                unsigned int *bytes_out, unsigned int *bytes_in,
                char **detail)
{
    te_string d = TE_STRING_INIT;
    te_errno rc = ta_upnp_ssdp_probe(st, mx, responders, bytes_out, bytes_in,
                                     &d);

    *detail = take(&d);
    return rc;
}

TARPC_FUNC_STATIC(upnp_ssdp_probe, {},
{
    int responders = 0;
    unsigned int bytes_out = 0;
    unsigned int bytes_in = 0;

    MAKE_CALL(out->retval = func(in->st, in->mx, &responders, &bytes_out,
                                 &bytes_in, &out->detail));
    out->responders = responders;
    out->bytes_out = bytes_out;
    out->bytes_in = bytes_in;
    out->common.errno_changed = false;
})
