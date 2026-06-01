# tsf-upnp

A UPnP control point for the OKTET Labs Test Environment (TE),
packaged as an external TE repository (consumed with the
`TE_EXT_REPO` builder directive). It drives UPnP from a Test Agent
over a low-level C library — no Python — for both quality verification
and security assessment.

Three libraries:

- `ta_upnp` — agent side. A UPnP control point over **libupnp
  (pupnp)**: SSDP discovery, downloading device and service
  descriptions, SOAP control actions, GENA eventing, and a raw
  M-SEARCH probe (a plain UDP socket, so the bytes it counts are the
  real ones on the wire). The agent and its RPC server both link it.
- `rpcs_upnp` — the `upnp_*` RPCs for the RPC server of the agent, thin
  wrappers over `ta_upnp`. Traffic originates on the agent, where the
  device under test is reachable, not on the engine.
- `tapi_upnp` — engine side. `tapi_upnp.h` gives a test discovery into
  a device list, description parsing into services and fields, SOAP
  actions with argument extraction, eventing, and the SSDP probe;
  `tapi_upnp_audit.h` reads a device as a security posture through
  tsf-cybersec.

TE has no UPnP of its own.

## Authorized use only

The security side — the SSDP amplification probe and the audit, above
all its port-mapping injection — acts against a real device. It is for
an **authorized** assessment: a pilot, a CTF, a device you own or are
engaged to test. The port-map injection adds a mapping and removes it
again, and runs only when you give it the agent's own LAN address.
Point it only at a device you are permitted to assess.

## Agent host requirements

- **libupnp (pupnp)** 1.14 with its development headers (Debian:
  `apt install libupnp-dev`; the `UpnpDiscovery_get_*_cstr` accessors
  used here are the 1.8+/1.14 API).
- The agent must sit on the same L2/L3 segment as the device, since
  SSDP is multicast to `239.255.255.250:1900`.

## Usage

Declare the repository in an external libraries catalog and pass it to
`dispatcher.sh --external=<catalog.yml>`:

```yaml
repositories:
  - name: tsf_upnp
    url: https://github.com/interpretica-io/tsf-upnp.git
    ref: v1.0.0
    libs:
      - ta_upnp
      - rpcs_upnp
      - tapi_upnp
```

In `builder.conf`, bind `tapi_upnp` to the engine, the agent libraries
to the agent platform, add the RPC definitions to `rpcxdr` on both, and
put `ta_upnp` with `rpcs_upnp` into the RPC server:

```
TE_EXT_REPO_USE([tsf_upnp], [], [tapi_upnp])
TE_LIB_PARMS([rpcxdr], [], [],
             [--with-rpcdefs=tarpc_job.x.m4,../ta_upnp/upnp_rpc.x.m4])

TE_EXT_REPO_USE([tsf_upnp], [<agent platform>], [ta_upnp rpcs_upnp])
TE_LIB_PARMS([rpcxdr], [<agent platform>], [],
             [--with-rpcdefs=tarpc_job.x.m4,../ta_upnp/upnp_rpc.x.m4])
TE_TA_TYPE([<ta type>], [<agent platform>], [unix], [--with-rcf-rpc],
           [], [], [], [comm_net_agent rcfpch ta_upnp])
TE_TA_APP([ta_rpcprovider], [<agent platform>], [<ta type>],
          [ta_rpcprovider], [], [],
          [... rpcs_job rpcs_upnp ta_upnp rpcserver agentlib rpcxdrta ...],
          [\${EXT_SOURCES}/build.sh], [ta_rpcs], [])
```

`rpcs_upnp` must come before `rpcxdrta` in the application's library
list: `tarpc.c` in `rpcxdrta` has a weak stub for every RPC and the
linker keeps the first definition it meets, so an `rpcs_*` library
listed after it never gets linked in and the RPCs fail with
`RPC-ERPCNOTSUPP`.

The RPC program number is **23** (`program upnp … = 23`). It must be
unique across every `.x.m4` in the build; the sibling modules use 20
(android), 21 (apple) and 22 (appium).

Add `tapi_upnp` to the `te_libs` of the suite.

## The control-point API

One handle, brought up with `tapi_upnp_start()` and down with
`tapi_upnp_stop()`; everything else names its target by URL.

- **Discovery** — `tapi_upnp_discover()` sends an M-SEARCH for a target
  (`"ssdp:all"`, `"upnp:rootdevice"`, a device or service type) and
  returns a vector of `tapi_upnp_device` (USN, ST, LOCATION, SERVER).
- **Description** — `tapi_upnp_describe()` downloads a device
  description; `tapi_upnp_xml_field()` reads a field
  (`friendlyName`, `manufacturer`, `modelNumber`, `serialNumber`,
  `UDN`); `tapi_upnp_services()` parses the service list into
  `tapi_upnp_service` with the control, event and SCPD URLs resolved
  against the description's base.
- **Control** — `tapi_upnp_action()` invokes a SOAP action with
  `"name=value"` arguments and returns the response, or a UPnP fault's
  errorCode/errorDescription; `tapi_upnp_out_arg()` reads a named
  out-argument out of the response.
- **Eventing** — `tapi_upnp_subscribe()`, `tapi_upnp_renew()`,
  `tapi_upnp_unsubscribe()` for the GENA path.
- **SSDP probe** — `tapi_upnp_ssdp_probe()` sends one raw M-SEARCH and
  reports the responder count and the bytes out and in, for the
  reflection/amplification posture.

The same primitives serve both jobs. **Quality**: discover a device,
check its descriptions parse and name the expected services, invoke
actions and read their results, exercise eventing — does the device's
UPnP actually work. **Security**: what that working UPnP exposes.

## What the posture is worth

`tapi_upnp_audit()` reads a device through tsf-cybersec.

| Finding | Severity | Read from |
|---|---|---|
| `upnp.ssdp-responds` | info | the device answers an M-SEARCH |
| `upnp.ssdp-amplification` | high | the answer is many times the request size |
| `upnp.info-leak` | low | a description exposes a serial or model number |
| `upnp.igd-present` | medium | an Internet Gateway connection service is reachable |
| `upnp.portmap-enumerable` | high | existing port mappings can be listed |
| `upnp.portmap-injectable` | critical | a LAN host opened a WAN port through the gateway |
| `upnp.not-assessed` | info | the control point could not run |

`upnp.portmap-injectable` is the one that matters most: it invokes
`AddPortMapping` from the LAN and, if it succeeds, any host on the
network can open a path from the Internet to an inside host with no
authentication — the long-standing IGD exposure. It needs the agent's
LAN address (`internal_client` in the policy) as the mapping's internal
client, and removes the mapping it added with `DeletePortMapping`.

## What was verified, and what was not

**The parsing and posture logic — validated.** The description reader
(`tapi_upnp_xml_field`, `tapi_upnp_services`), the URL resolver (a
relative `controlURL` against the LOCATION's origin, an absolute one
kept), the SOAP out-argument reader, and the amplification ratio were
run against a realistic IGD root description and a SOAP response and
gave the right services, URLs, fields and verdict.

**The libupnp calls — written from the pupnp API, not yet run.** The
`ta_upnp` client was written against the documented pupnp 1.14 control
API (`UpnpInit2`, `UpnpSearchAsync` with the discovery callback,
`UpnpDownloadXmlDoc`, `UpnpSendAction`, `UpnpSubscribe`/`Renew`/
`UnSubscribe`) and the raw SSDP probe against plain BSD sockets. libupnp
was not installed in the authoring environment, so this half was not
compiled against the real headers; an accessor renamed between pupnp
versions is the first thing to check on a live build.

**The RPC layer — by construction.** `upnp_rpc.x.m4`, `rpcs_upnp` and
`tapi_upnp_rpc` follow the tsf-appium template exactly; they compile
only in the full TE build, which generates `tarpc.h` from the `.x.m4`.
Nothing here has yet run through a Test Agent.

## Scope

- **SSDP is multicast and the audit writes.** Discovery and the probe
  send to the whole segment; the audit's injection adds a real port
  mapping (then removes it). Run the security side only where you are
  authorized.
- **The control point changes the agent** while it is up; `tapi_upnp_stop()`
  takes it down, and a test that starts it should stop it in cleanup.
- **UPnP has no authentication by design.** That the actions here need
  no credentials is not a flaw this library introduces; it is what the
  audit is measuring the consequences of.
