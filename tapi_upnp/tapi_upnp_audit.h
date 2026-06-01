/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief What a device's UPnP is worth as a security posture
 *
 * @defgroup tapi_upnp_audit UPnP security posture
 * @ingroup tapi_upnp
 * @{
 *
 * A device's UPnP read as a security posture and reported through
 * tsf-cybersec: does SSDP reflect and amplify, does a description leak
 * a precise fingerprint, is there an Internet Gateway control service,
 * can its existing port mappings be listed, and - the classic hole -
 * can any host on the LAN open a port to the WAN through it.
 *
 * | Finding | Severity | Read from |
 * |---|---|---|
 * | @c upnp.ssdp-responds | info | the device answers an M-SEARCH |
 * | @c upnp.ssdp-amplification | high | the answer is many times the request |
 * | @c upnp.info-leak | low | a description exposes a serial or model number |
 * | @c upnp.igd-present | medium | an Internet Gateway control service is reachable |
 * | @c upnp.portmap-enumerable | high | existing port mappings can be listed |
 * | @c upnp.portmap-injectable | critical | a LAN host can open a WAN port through it |
 * | @c upnp.not-assessed | info | the control point could not run |
 *
 * The port-map injection actually adds a mapping and then removes it,
 * so run this only against a device you are authorized to assess, and
 * only when @a internal_client is set to the agent's own LAN address -
 * without it the injection cannot be tested and is skipped.
 */

#ifndef __TAPI_UPNP_AUDIT_H__
#define __TAPI_UPNP_AUDIT_H__

#include "te_errno.h"
#include "rcf_rpc.h"

#include "tapi_cybersec.h"
#include "tapi_upnp.h"

#ifdef __cplusplus
extern "C" {
#endif

/** What the device's UPnP is expected to be. */
typedef struct tapi_upnp_audit_policy {
    /** A device answering SSDP at all is acceptable (it usually is). */
    bool allow_ssdp;
    /** The bytes-in / bytes-out ratio that counts as amplification. */
    unsigned int amplification_min;
    /** An Internet Gateway control service is acceptable. */
    bool allow_igd;
    /**
     * Try to add a port mapping from the LAN - the injection check. It
     * needs @a internal_client; without it the check is skipped whatever
     * this says.
     */
    bool attempt_portmap;
    /** List existing port mappings (GetGenericPortMappingEntry). */
    bool enumerate_portmaps;
    /**
     * The agent's own LAN address, the @c NewInternalClient an
     * AddPortMapping needs, or @c NULL to skip the injection check.
     */
    const char *internal_client;
    /** External/internal port the injection test uses; @c 0 for 34999. */
    unsigned int test_port;
} tapi_upnp_audit_policy;

/**
 * The default: SSDP allowed, amplification at 5x, IGD not allowed,
 * injection attempted (needs @a internal_client), mappings enumerated.
 */
extern const tapi_upnp_audit_policy tapi_upnp_default_audit_policy;

/**
 * Read a device's UPnP posture into @p report.
 *
 * @param[in]  rpcs     RPC server on the agent (its control point is
 *                      started if it is not already).
 * @param[in]  mx       MX and the seconds each discovery collects.
 * @param[in]  policy   What is expected, or @c NULL for the default.
 * @param[out] report   Report to append findings to.
 *
 * @return Status code of reading the posture, not its verdict.
 */
extern te_errno tapi_upnp_audit(rcf_rpc_server *rpcs, int mx,
                                const tapi_upnp_audit_policy *policy,
                                tapi_cybersec_report *report);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TAPI_UPNP_AUDIT_H__ */

/**@} <!-- END tapi_upnp_audit --> */
