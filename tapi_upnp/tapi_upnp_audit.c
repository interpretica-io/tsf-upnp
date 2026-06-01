/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief A device's UPnP read as a security posture
 *
 * SSDP amplification first (a raw probe), then discovery and each
 * description: a fingerprint leak, an Internet Gateway control service,
 * whether its port mappings can be listed, and whether a LAN host can
 * open a WAN port through it. The injection check adds a mapping and
 * removes it again; it runs only when the policy gives the agent's LAN
 * address to use as the internal client.
 */

#define TE_LGR_USER     "TAPI UPnP audit"

#include "te_config.h"

#include <string.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_str.h"
#include "te_string.h"
#include "te_vector.h"
#include "logger_api.h"

#include "tapi_cybersec.h"
#include "tapi_upnp.h"
#include "tapi_upnp_audit.h"

/* See description in tapi_upnp_audit.h */
const tapi_upnp_audit_policy tapi_upnp_default_audit_policy = {
    .allow_ssdp = true,
    .amplification_min = 5,
    .allow_igd = false,
    .attempt_portmap = true,
    .enumerate_portmaps = true,
    .internal_client = NULL,
    .test_port = 0,
};

/** Is this service type an Internet Gateway connection service? */
static bool
audit_is_igd(const char *service_type)
{
    return service_type != NULL &&
           (strstr(service_type, "WANIPConnection") != NULL ||
            strstr(service_type, "WANPPPConnection") != NULL);
}

/** The SSDP reflection/amplification posture from a raw probe. */
static void
audit_ssdp(rcf_rpc_server *rpcs, int mx,
           const tapi_upnp_audit_policy *policy, tapi_cybersec_report *report)
{
    int responders = 0;
    unsigned int out = 0;
    unsigned int in = 0;

    if (tapi_upnp_ssdp_probe(rpcs, "ssdp:all", mx, &responders, &out, &in,
                             NULL) != 0)
        return;

    if (responders > 0 && !policy->allow_ssdp)
    {
        tapi_cybersec_report_add(report, TAPI_CYBERSEC_SEV_INFO,
                                 "upnp.ssdp-responds", "ssdp",
                                 "The device answers an SSDP M-SEARCH");
    }
    if (out > 0 && in >= policy->amplification_min * out)
    {
        tapi_cybersec_report_add(report, TAPI_CYBERSEC_SEV_HIGH,
                                 "upnp.ssdp-amplification", "ssdp",
                                 "An SSDP M-SEARCH draws an answer about "
                                 "%ux its own size, so the device can reflect "
                                 "and amplify a spoofed request",
                                 out != 0 ? in / out : 0);
    }
}

/** Run one IGD action, returning whether it succeeded (no fault). */
static bool
audit_igd_action(rcf_rpc_server *rpcs, const tapi_upnp_service *svc,
                 const char *action, const char *args)
{
    te_string resp = TE_STRING_INIT;
    te_string fault = TE_STRING_INIT;
    te_errno rc;

    rc = tapi_upnp_action(rpcs, svc->control_url, svc->service_type, action,
                          args, &resp, &fault);
    te_string_free(&resp);
    te_string_free(&fault);

    return rc == 0;
}

/** The Internet Gateway checks against one WAN connection service. */
static void
audit_igd(rcf_rpc_server *rpcs, const tapi_upnp_service *svc,
          const tapi_upnp_audit_policy *policy, tapi_cybersec_report *report)
{
    unsigned int port = policy->test_port != 0 ? policy->test_port : 34999;

    if (!policy->allow_igd)
    {
        tapi_cybersec_report_add(report, TAPI_CYBERSEC_SEV_MEDIUM,
                                 "upnp.igd-present", svc->service_type,
                                 "An Internet Gateway connection service is "
                                 "reachable and needs no authentication");
    }

    /* Existing mappings listed -> the internal hosts and ports leak. */
    if (policy->enumerate_portmaps &&
        audit_igd_action(rpcs, svc, "GetGenericPortMappingEntry",
                         "NewPortMappingIndex=0"))
    {
        tapi_cybersec_report_add(report, TAPI_CYBERSEC_SEV_HIGH,
                                 "upnp.portmap-enumerable", svc->service_type,
                                 "Existing port mappings can be listed, "
                                 "leaking internal hosts and ports");
    }

    /* The classic hole: any LAN host opens a WAN port. Added, then removed. */
    if (policy->attempt_portmap && policy->internal_client != NULL)
    {
        te_string add = TE_STRING_INIT;
        te_string del = TE_STRING_INIT;

        te_string_append(&add,
                         "NewRemoteHost=\n"
                         "NewExternalPort=%u\n"
                         "NewProtocol=TCP\n"
                         "NewInternalPort=%u\n"
                         "NewInternalClient=%s\n"
                         "NewEnabled=1\n"
                         "NewPortMappingDescription=tsf-upnp-audit\n"
                         "NewLeaseDuration=0",
                         port, port, policy->internal_client);

        if (audit_igd_action(rpcs, svc, "AddPortMapping", add.ptr))
        {
            tapi_cybersec_report_add(report, TAPI_CYBERSEC_SEV_CRITICAL,
                                     "upnp.portmap-injectable",
                                     svc->service_type,
                                     "A host on the LAN opened a WAN port "
                                     "through the gateway with no "
                                     "authentication");

            te_string_append(&del,
                             "NewRemoteHost=\n"
                             "NewExternalPort=%u\n"
                             "NewProtocol=TCP",
                             port);
            audit_igd_action(rpcs, svc, "DeletePortMapping", del.ptr);
        }

        te_string_free(&add);
        te_string_free(&del);
    }
}

/* See description in tapi_upnp_audit.h */
te_errno
tapi_upnp_audit(rcf_rpc_server *rpcs, int mx,
                const tapi_upnp_audit_policy *policy,
                tapi_cybersec_report *report)
{
    te_vec devices = TE_VEC_INIT(tapi_upnp_device);
    tapi_upnp_device *dev;
    bool leaked = false;
    te_errno rc;

    if (report == NULL)
        return TE_RC(TE_TAPI, TE_EINVAL);
    if (policy == NULL)
        policy = &tapi_upnp_default_audit_policy;
    if (mx <= 0)
        mx = 3;

    rc = tapi_upnp_start(rpcs, NULL);
    if (rc != 0)
    {
        tapi_cybersec_report_add(report, TAPI_CYBERSEC_SEV_INFO,
                                 "upnp.not-assessed", "upnp",
                                 "The control point could not be started");
        return 0;
    }

    audit_ssdp(rpcs, mx, policy, report);

    rc = tapi_upnp_discover(rpcs, "ssdp:all", mx, &devices);
    if (rc != 0)
    {
        tapi_upnp_devices_free(&devices);
        return rc;
    }

    TE_VEC_FOREACH(&devices, dev)
    {
        te_string xml = TE_STRING_INIT;
        te_vec services = TE_VEC_INIT(tapi_upnp_service);
        tapi_upnp_service *svc;

        if (dev->location == NULL || *dev->location == '\0')
        {
            te_string_free(&xml);
            continue;
        }
        if (tapi_upnp_describe(rpcs, dev->location, &xml) != 0)
        {
            te_string_free(&xml);
            continue;
        }

        /* A serial or model number is a precise fingerprint; report once. */
        if (!leaked &&
            (tapi_upnp_xml_field(xml.ptr, "serialNumber", NULL) ||
             tapi_upnp_xml_field(xml.ptr, "modelNumber", NULL)))
        {
            tapi_cybersec_report_add(report, TAPI_CYBERSEC_SEV_LOW,
                                     "upnp.info-leak", dev->location,
                                     "A device description exposes a serial "
                                     "or model number, aiding fingerprinting");
            leaked = true;
        }

        if (tapi_upnp_services(xml.ptr, dev->location, &services) == 0)
        {
            TE_VEC_FOREACH(&services, svc)
            {
                if (audit_is_igd(svc->service_type))
                    audit_igd(rpcs, svc, policy, report);
            }
        }

        tapi_upnp_services_free(&services);
        te_string_free(&xml);
    }

    tapi_upnp_devices_free(&devices);

    return 0;
}
