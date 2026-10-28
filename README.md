# vpp-tenantcount

A small FD.io VPP plugin: per-tenant packet/byte counters on the `ip4-unicast`
feature arc. It is deliberately tiny, but it exercises the real plugin
anatomy used by production VPP data planes:

- a graph node (`node.c`) inserted into a feature arc before `ip4-lookup`
- per-thread-safe combined counters (packets + bytes)
- CLI to map an interface to a tenant and to show/clear counters
- trace output (`trace add af-packet-input 10; show trace`)
- an end-to-end test with Linux network namespaces (`test/netns-test.sh`)

It is a stepping stone toward multi-tenant sandbox networking (one tenant
per customer, many microVM interfaces per tenant). See milestones below.

## Build & run (Ubuntu 22.04/24.04, x86_64, root needed for the test)

    scripts/build.sh             # clones FD.io VPP, installs deps, builds debug
    scripts/run-vpp.sh           # terminal 1
    test/netns-test.sh           # terminal 2, expects "PASS"

Manual CLI:

    sudo vppctl -s /run/vpp/cli.sock
    vpp# tenantcount assign host-vethA-h tenant 1
    vpp# show tenantcount
    vpp# trace add af-packet-input 5
    vpp# show trace

## Layout

    plugin/CMakeLists.txt   add_vpp_plugin(...)
    plugin/tenantcount.h    shared types, error/next enums
    plugin/tenantcount.c    init, CLI, feature arc registration
    plugin/node.c           the graph node (single-packet loop)
    scripts/                build / run helpers + startup.conf
    test/netns-test.sh      namespace-based end-to-end test

## Milestones (what makes this portfolio-grade)

1. **This scaffold works.** Build, run, test passes. Read `node.c` until you
   can explain every line (frames, `vlib_get_next_frame`, `vnet_feature_next`).
2. **Binary API + Go client.** Add `tenantcount.api` (see
   `src/examples/sample-plugin` in the VPP tree), then drive it from Go with
   GoVPP: `AssignInterface(sw_if_index, tenant)`. This is the
   "network as a programmable API" story.
3. **Performance.** Rewrite the node loop as dual/quad with prefetch, then
   benchmark (TRex or `pktgen`) and publish before/after Mpps in this README.
   Use `show runtime` (clocks/packet) to prove it.
4. **Isolation.** Replace counting with per-tenant FIB tables (VRFs) or a
   GENEVE/VXLAN tunnel per tenant, and prove tenant A cannot reach tenant B.
5. **Upstream.** Fix a doc or test issue in FD.io's Gerrit and link the
   merged change in your README.

## Honest limits

- Counting only; no forwarding policy or isolation yet (milestone 4).
- IPv4 only; IPv6 arc (`ip6-unicast`) is a copy-paste extension.
- **Not compiled or run yet.** I only cross-checked the VPP calls used here
  (counters, feature arc, CLI parsing, error codes) against the headers in the
  `stable/2506` branch. Expect to fix small build errors on first compile;
  compare against `src/examples/sample-plugin` in your VPP checkout.
- VPP APIs shift between releases, so pin the branch you build against.
