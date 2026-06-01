/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief UPnP TAPI: RPC client wrappers
 *
 * Client wrappers of the upnp_* RPCs, see upnp_rpc.x.m4. Tests use
 * tapi_upnp.h; these are the calls behind it, one per RPC.
 */

#ifndef __TAPI_UPNP_RPC_H__
#define __TAPI_UPNP_RPC_H__

#include "te_errno.h"
#include "te_string.h"
#include "rcf_rpc.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Bring the control point up on an interface. */
extern te_errno rpc_upnp_init(rcf_rpc_server *rpcs, const char *iface,
                              int port);

/** Take the control point down. */
extern te_errno rpc_upnp_finish(rcf_rpc_server *rpcs);

/** An M-SEARCH; @p result is the tab/newline responder text, @p count its rows. */
extern te_errno rpc_upnp_discover(rcf_rpc_server *rpcs, const char *st,
                                  int mx, int *count, te_string *result);

/** Download a device description from its LOCATION. */
extern te_errno rpc_upnp_describe(rcf_rpc_server *rpcs, const char *location,
                                  te_string *xml);

/** Plain HTTP GET of a URL (an SCPD, say). */
extern te_errno rpc_upnp_get(rcf_rpc_server *rpcs, const char *url,
                             te_string *body);

/** Invoke a SOAP action; @p args is "name=value" lines. */
extern te_errno rpc_upnp_action(rcf_rpc_server *rpcs, const char *control_url,
                                const char *service_type, const char *action,
                                const char *args, te_string *response,
                                te_string *fault);

/** Subscribe to an eventing URL. */
extern te_errno rpc_upnp_subscribe(rcf_rpc_server *rpcs,
                                   const char *event_url, int timeout,
                                   te_string *sid, int *actual_timeout);

/** Renew a subscription by SID. */
extern te_errno rpc_upnp_renew(rcf_rpc_server *rpcs, const char *sid,
                               int timeout, int *actual_timeout);

/** End a subscription by SID. */
extern te_errno rpc_upnp_unsubscribe(rcf_rpc_server *rpcs, const char *sid);

/** Send one raw M-SEARCH and measure the answer. */
extern te_errno rpc_upnp_ssdp_probe(rcf_rpc_server *rpcs, const char *st,
                                    int mx, int *responders,
                                    unsigned int *bytes_out,
                                    unsigned int *bytes_in, te_string *detail);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TAPI_UPNP_RPC_H__ */
