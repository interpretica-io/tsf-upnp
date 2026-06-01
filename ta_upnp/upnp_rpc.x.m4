/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief RPC for UPnP control-point operations
 *
 * The RPCs of rpcs_upnp, a control point over libupnp (pupnp) plus a
 * raw SSDP probe. Add this file to the rpcxdr definitions of the
 * engine platform and of the agent platform:
 *
 *   TE_LIB_PARMS([rpcxdr], [<platform>], [],
 *                [--with-rpcdefs=tarpc_job.x.m4,../ta_upnp/upnp_rpc.x.m4])
 *
 * The control point lives in the RPC server process; upnp_init() brings
 * it up on an interface and upnp_finish() takes it down. Results that
 * are lists come back as newline-separated text, one record per line
 * with tab-separated fields, the same shape tsf-appium uses - the
 * engine side parses them. A device or service is named by URL on every
 * call, so no handle has to survive between RPCs.
 */

/* upnp_init(): bring the control point up on an interface. */
struct tarpc_upnp_init_in {
    struct tarpc_in_arg common;

    string iface<>;         /* interface name, or "" for auto */
    tarpc_int port;         /* local port, or 0 for auto */
};

struct tarpc_upnp_init_out {
    struct tarpc_out_arg common;

    tarpc_int retval;
};

/* upnp_finish(): take the control point down. */
struct tarpc_upnp_finish_in {
    struct tarpc_in_arg common;
};

struct tarpc_upnp_finish_out {
    struct tarpc_out_arg common;

    tarpc_int retval;
};

/*
 * upnp_discover(): an M-SEARCH for a target, the responses collected
 * over mx seconds. result is one responder per line:
 *   USN \t ST \t LOCATION \t SERVER
 */
struct tarpc_upnp_discover_in {
    struct tarpc_in_arg common;

    string st<>;            /* search target, e.g. "ssdp:all" */
    tarpc_int mx;           /* MX: seconds a responder may wait */
};

struct tarpc_upnp_discover_out {
    struct tarpc_out_arg common;

    tarpc_int retval;
    tarpc_int count;
    string result<>;
};

/* upnp_describe(): download a device description from its LOCATION. */
struct tarpc_upnp_describe_in {
    struct tarpc_in_arg common;

    string location<>;
};

struct tarpc_upnp_describe_out {
    struct tarpc_out_arg common;

    tarpc_int retval;
    string xml<>;
};

/* upnp_get(): a plain HTTP GET, for an SCPD or any description URL. */
struct tarpc_upnp_get_in {
    struct tarpc_in_arg common;

    string url<>;
};

struct tarpc_upnp_get_out {
    struct tarpc_out_arg common;

    tarpc_int retval;
    string body<>;
};

/*
 * upnp_action(): invoke a SOAP action. args is "name=value" lines. On
 * success response is the SOAP response body; on a UPnP fault, fault
 * is the errorCode and errorDescription and retval is non-zero.
 */
struct tarpc_upnp_action_in {
    struct tarpc_in_arg common;

    string control_url<>;
    string service_type<>;
    string action<>;
    string args<>;
};

struct tarpc_upnp_action_out {
    struct tarpc_out_arg common;

    tarpc_int retval;
    string response<>;
    string fault<>;
};

/* upnp_subscribe(): a GENA subscription to an eventing URL. */
struct tarpc_upnp_subscribe_in {
    struct tarpc_in_arg common;

    string event_url<>;
    tarpc_int timeout;      /* requested seconds, or 0 for infinite */
};

struct tarpc_upnp_subscribe_out {
    struct tarpc_out_arg common;

    tarpc_int retval;
    string sid<>;
    tarpc_int actual_timeout;
};

/* upnp_renew(): renew a subscription by SID. */
struct tarpc_upnp_renew_in {
    struct tarpc_in_arg common;

    string sid<>;
    tarpc_int timeout;
};

struct tarpc_upnp_renew_out {
    struct tarpc_out_arg common;

    tarpc_int retval;
    tarpc_int actual_timeout;
};

/* upnp_unsubscribe(): end a subscription by SID. */
struct tarpc_upnp_unsubscribe_in {
    struct tarpc_in_arg common;

    string sid<>;
};

struct tarpc_upnp_unsubscribe_out {
    struct tarpc_out_arg common;

    tarpc_int retval;
};

/*
 * upnp_ssdp_probe(): send one raw M-SEARCH and measure the answer -
 * how many distinct responders, how many bytes went out and came back,
 * for the SSDP reflection/amplification posture. responders is one
 * "ADDR \t BYTES" line per responder.
 */
struct tarpc_upnp_ssdp_probe_in {
    struct tarpc_in_arg common;

    string st<>;
    tarpc_int mx;
};

struct tarpc_upnp_ssdp_probe_out {
    struct tarpc_out_arg common;

    tarpc_int retval;
    tarpc_int responders;
    tarpc_uint bytes_out;
    tarpc_uint bytes_in;
    string detail<>;
};

program upnp
{
    version ver0
    {
        RPC_DEF(upnp_init)
        RPC_DEF(upnp_finish)
        RPC_DEF(upnp_discover)
        RPC_DEF(upnp_describe)
        RPC_DEF(upnp_get)
        RPC_DEF(upnp_action)
        RPC_DEF(upnp_subscribe)
        RPC_DEF(upnp_renew)
        RPC_DEF(upnp_unsubscribe)
        RPC_DEF(upnp_ssdp_probe)
    } = 1;
} = 23;
