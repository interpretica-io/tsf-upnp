/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief UPnP TAPI
 *
 * @defgroup tapi_upnp UPnP control point (tapi_upnp)
 * @{
 *
 * A UPnP control point a test drives on an agent: SSDP discovery, the
 * device and service descriptions, SOAP control actions, GENA
 * eventing, and a raw SSDP amplification probe. It runs in the agent's
 * RPC server over libupnp (pupnp); this is the engine-side face of the
 * @c upnp_* RPCs, with the list and XML parsing done here so a test
 * gets devices, services and argument values rather than raw text.
 *
 * Two uses in one API. **Quality**: discover a device, fetch and check
 * its descriptions, invoke actions and read their results, and exercise
 * eventing - does the device's UPnP actually work. **Security**: the
 * same primitives, plus the SSDP probe and the audit, ask what that
 * working UPnP exposes - an Internet Gateway that any LAN host can open
 * ports on, SSDP that reflects and amplifies, descriptions that leak a
 * precise fingerprint. Point the security side only at a device you are
 * authorized to assess.
 *
 * @code
 * te_vec devices = TE_VEC_INIT(tapi_upnp_device);
 *
 * CHECK_RC(tapi_upnp_start(rpcs, NULL));
 * CHECK_RC(tapi_upnp_discover(rpcs, "upnp:rootdevice", 3, &devices));
 * @endcode
 */

#ifndef __TAPI_UPNP_H__
#define __TAPI_UPNP_H__

#include "te_errno.h"
#include "te_string.h"
#include "te_vector.h"
#include "rcf_rpc.h"

#include "tapi_upnp_rpc.h"

#ifdef __cplusplus
extern "C" {
#endif

/** A device or service as SSDP discovery reported it. */
typedef struct tapi_upnp_device {
    /** Unique Service Name (the @c USN header). */
    char *usn;
    /** Search target it answered for (its device or service type). */
    char *st;
    /** The @c LOCATION URL of its description. */
    char *location;
    /** The @c SERVER header (the stack and OS it named). */
    char *server;
} tapi_upnp_device;

/** A service as a device description lists it. */
typedef struct tapi_upnp_service {
    /** Service type, e.g. @c "urn:schemas-upnp-org:service:WANIPConnection:1". */
    char *service_type;
    /** Service id. */
    char *service_id;
    /** Control URL, resolved against the description's base. */
    char *control_url;
    /** Eventing (GENA) URL, resolved against the base. */
    char *event_sub_url;
    /** Service description (SCPD) URL, resolved against the base. */
    char *scpd_url;
} tapi_upnp_service;

/**
 * Bring the agent's control point up.
 *
 * @param rpcs      RPC server on the agent.
 * @param iface     Interface to bind, or @c NULL for libupnp's choice.
 *
 * @return Status code.
 */
extern te_errno tapi_upnp_start(rcf_rpc_server *rpcs, const char *iface);

/** Take the agent's control point down. */
extern te_errno tapi_upnp_stop(rcf_rpc_server *rpcs);

/**
 * Discover devices and services with an M-SEARCH.
 *
 * @param[in]  rpcs     RPC server on the agent.
 * @param[in]  st       Search target (@c "ssdp:all", a device type, …).
 * @param[in]  mx       MX and the seconds to collect answers.
 * @param[out] devices  Vector of #tapi_upnp_device to append to;
 *                      release with tapi_upnp_devices_free().
 *
 * @return Status code.
 */
extern te_errno tapi_upnp_discover(rcf_rpc_server *rpcs, const char *st,
                                   int mx, te_vec *devices);

/** Release a vector of devices. */
extern void tapi_upnp_devices_free(te_vec *devices);

/**
 * Download a device description from its @c LOCATION.
 *
 * @param[in]  rpcs     RPC server on the agent.
 * @param[in]  location The @c LOCATION URL.
 * @param[out] xml      The description XML.
 *
 * @return Status code.
 */
extern te_errno tapi_upnp_describe(rcf_rpc_server *rpcs, const char *location,
                                   te_string *xml);

/**
 * Read the first @c <tag>value</tag> out of a description or response.
 *
 * A small, namespace-agnostic lookup for a field such as
 * @c friendlyName, @c manufacturer, @c modelNumber, @c serialNumber or
 * @c UDN, or for a SOAP out-argument.
 *
 * @param[in]  xml      The XML.
 * @param[in]  tag      The element's local name.
 * @param[out] value    String to append the text content to.
 *
 * @return @c true when the tag was found.
 */
extern bool tapi_upnp_xml_field(const char *xml, const char *tag,
                                te_string *value);

/**
 * Parse the services out of a device description.
 *
 * @param[in]  xml      The description XML.
 * @param[in]  base_url The description's @c LOCATION, to resolve the
 *                      relative control, event and SCPD URLs against.
 * @param[out] services Vector of #tapi_upnp_service to append to;
 *                      release with tapi_upnp_services_free().
 *
 * @return Status code.
 */
extern te_errno tapi_upnp_services(const char *xml, const char *base_url,
                                   te_vec *services);

/** Release a vector of services. */
extern void tapi_upnp_services_free(te_vec *services);

/**
 * Invoke a SOAP control action.
 *
 * @param[in]  rpcs         RPC server on the agent.
 * @param[in]  control_url  The service's control URL.
 * @param[in]  service_type The service type.
 * @param[in]  action       Action name.
 * @param[in]  args         Arguments as @c "name=value" lines, or @c NULL.
 * @param[out] response     SOAP response body on success, or @c NULL.
 * @param[out] fault        errorCode/errorDescription on a fault, or @c NULL.
 *
 * @return Status code; a UPnP fault is non-zero with @p fault set.
 */
extern te_errno tapi_upnp_action(rcf_rpc_server *rpcs, const char *control_url,
                                 const char *service_type, const char *action,
                                 const char *args, te_string *response,
                                 te_string *fault);

/** Read a named out-argument out of a SOAP action response. */
extern bool tapi_upnp_out_arg(const char *response, const char *name,
                              te_string *value);

/** Subscribe to a service's eventing URL. */
extern te_errno tapi_upnp_subscribe(rcf_rpc_server *rpcs,
                                    const char *event_url, int timeout,
                                    te_string *sid, int *actual_timeout);

/** Renew a subscription by SID. */
extern te_errno tapi_upnp_renew(rcf_rpc_server *rpcs, const char *sid,
                                int timeout, int *actual_timeout);

/** End a subscription by SID. */
extern te_errno tapi_upnp_unsubscribe(rcf_rpc_server *rpcs, const char *sid);

/**
 * Send one raw M-SEARCH and measure the answer.
 *
 * @param[in]  rpcs         RPC server on the agent.
 * @param[in]  st           Search target.
 * @param[in]  mx           MX and the seconds to collect.
 * @param[out] responders   Number of distinct responders, or @c NULL.
 * @param[out] bytes_out    Bytes of the request, or @c NULL.
 * @param[out] bytes_in     Bytes of all answers, or @c NULL.
 * @param[out] detail       Per-responder @c "ADDR\\tBYTES" lines, or @c NULL.
 *
 * @return Status code.
 */
extern te_errno tapi_upnp_ssdp_probe(rcf_rpc_server *rpcs, const char *st,
                                     int mx, int *responders,
                                     unsigned int *bytes_out,
                                     unsigned int *bytes_in,
                                     te_string *detail);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TAPI_UPNP_H__ */

/**@} <!-- END tapi_upnp --> */
