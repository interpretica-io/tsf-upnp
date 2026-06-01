/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Agent-side UPnP control point
 *
 * A UPnP control point on top of libupnp (pupnp): SSDP discovery, the
 * device and service descriptions, SOAP control actions, GENA
 * eventing, and a raw M-SEARCH probe for the SSDP reflection posture.
 * The agent and its RPC server both link this; the RPCs (see
 * upnp_rpc.x.m4) are thin wrappers over these functions.
 *
 * The control point is a single global handle brought up by
 * ta_upnp_init() and torn down by ta_upnp_finish(). Every other call
 * names its target by URL, so nothing has to survive between calls but
 * the handle.
 */

#ifndef __TA_UPNP_H__
#define __TA_UPNP_H__

#include "te_errno.h"
#include "te_string.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Bring the control point up on an interface.
 *
 * Idempotent: a second call while it is up is a no-op that succeeds.
 *
 * @param iface     Interface name, or @c NULL / @c "" for libupnp's
 *                  own choice.
 * @param port      Local port, or @c 0 to let libupnp pick.
 *
 * @return Status code.
 */
extern te_errno ta_upnp_init(const char *iface, int port);

/** Take the control point down. */
extern te_errno ta_upnp_finish(void);

/**
 * Discover devices and services with an SSDP M-SEARCH.
 *
 * @param[in]  st       Search target (@c "ssdp:all",
 *                      @c "upnp:rootdevice", a device or service type).
 * @param[in]  mx       MX: the seconds a responder may spread its
 *                      answer over; also how long this collects.
 * @param[out] count    Number of responders.
 * @param[out] result   One responder per line, tab-separated:
 *                      @c "USN\\tST\\tLOCATION\\tSERVER".
 *
 * @return Status code.
 */
extern te_errno ta_upnp_discover(const char *st, int mx, int *count,
                                 te_string *result);

/**
 * Download a device description document from its @c LOCATION.
 *
 * @param[in]  location     The @c LOCATION URL from discovery.
 * @param[out] xml          The description XML.
 *
 * @return Status code.
 */
extern te_errno ta_upnp_describe(const char *location, te_string *xml);

/**
 * Plain HTTP GET, for a service description (SCPD) or any URL a
 * description points at.
 *
 * @param[in]  url      The URL.
 * @param[out] body     The body.
 *
 * @return Status code.
 */
extern te_errno ta_upnp_get(const char *url, te_string *body);

/**
 * Invoke a SOAP control action.
 *
 * @param[in]  control_url      The service's @c controlURL.
 * @param[in]  service_type     The service type (the SOAP namespace).
 * @param[in]  action           Action name.
 * @param[in]  args             Arguments as @c "name=value" lines, or
 *                              @c NULL / @c "" for none.
 * @param[out] response         The SOAP response body on success.
 * @param[out] fault            On a UPnP fault, its errorCode and
 *                              errorDescription; empty otherwise.
 *
 * @return Status code; a UPnP fault is a non-zero code with @p fault set.
 */
extern te_errno ta_upnp_action(const char *control_url,
                               const char *service_type, const char *action,
                               const char *args, te_string *response,
                               te_string *fault);

/**
 * Subscribe to a service's eventing (GENA) URL.
 *
 * @param[in]  event_url        The service's @c eventSubURL.
 * @param[in]  timeout          Requested seconds, or @c 0 for infinite.
 * @param[out] sid              The subscription id the server gave.
 * @param[out] actual_timeout   The timeout the server granted.
 *
 * @return Status code.
 */
extern te_errno ta_upnp_subscribe(const char *event_url, int timeout,
                                  te_string *sid, int *actual_timeout);

/** Renew a subscription by its SID. */
extern te_errno ta_upnp_renew(const char *sid, int timeout,
                              int *actual_timeout);

/** End a subscription by its SID. */
extern te_errno ta_upnp_unsubscribe(const char *sid);

/**
 * Send one raw M-SEARCH and measure the answer.
 *
 * Bypasses libupnp so the bytes in and out are the real UDP ones: the
 * measurement behind the SSDP reflection/amplification posture.
 *
 * @param[in]  st           Search target.
 * @param[in]  mx           MX and the seconds to collect answers.
 * @param[out] responders   Number of distinct responders.
 * @param[out] bytes_out    Bytes of the request sent.
 * @param[out] bytes_in     Bytes of all answers.
 * @param[out] detail       One @c "ADDR\\tBYTES" line per responder.
 *
 * @return Status code.
 */
extern te_errno ta_upnp_ssdp_probe(const char *st, int mx, int *responders,
                                   unsigned int *bytes_out,
                                   unsigned int *bytes_in, te_string *detail);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TA_UPNP_H__ */
