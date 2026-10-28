#!/usr/bin/env bash
# End-to-end check: two "tenants" (network namespaces) ping VPP over af_packet
# interfaces; we then verify per-tenant counters. Needs root and a running VPP
# (scripts/run-vpp.sh in another terminal).
set -euo pipefail
VPPCTL="${VPPCTL:-sudo vppctl -s /run/vpp/cli.sock}"

cleanup() {
  for ns in tenant-a tenant-b; do sudo ip netns del "$ns" 2>/dev/null || true; done
  sudo ip link del vethA-h 2>/dev/null || true
  sudo ip link del vethB-h 2>/dev/null || true
}
trap cleanup EXIT
cleanup

setup_tenant() { # name hostif nsif subnet-prefix
  local ns="$1" hostif="$2" nsif="$3" net="$4"
  sudo ip netns add "$ns"
  sudo ip link add "$hostif" type veth peer name "$nsif"
  sudo ip link set "$nsif" netns "$ns"
  sudo ip netns exec "$ns" ip addr add "${net}.2/24" dev "$nsif"
  sudo ip netns exec "$ns" ip link set "$nsif" up
  sudo ip netns exec "$ns" ip link set lo up
  sudo ip link set "$hostif" up
}
setup_tenant tenant-a vethA-h vethA-n 10.1.0
setup_tenant tenant-b vethB-h vethB-n 10.2.0

# VPP side
$VPPCTL create host-interface name vethA-h
$VPPCTL create host-interface name vethB-h
for i in A B; do $VPPCTL set interface state host-veth${i}-h up; done
$VPPCTL set interface ip address host-vethA-h 10.1.0.1/24
$VPPCTL set interface ip address host-vethB-h 10.2.0.1/24
$VPPCTL tenantcount assign host-vethA-h tenant 1
$VPPCTL tenantcount assign host-vethB-h tenant 2
$VPPCTL clear tenantcount

# Tenant A sends 5 pings, tenant B sends 10.
sudo ip netns exec tenant-a ping -c 5  -W 1 10.1.0.1 >/dev/null
sudo ip netns exec tenant-b ping -c 10 -W 1 10.2.0.1 >/dev/null

OUT="$($VPPCTL show tenantcount)"
echo "$OUT"

# Each echo request is 98 bytes on the wire at IP level (84 IP + 14 eth) -> we
# only assert on packet counts, which are exact for a quiet link.
echo "$OUT" | grep -q "tenant 1: 5 packets"  || { echo "FAIL tenant 1"; exit 1; }
echo "$OUT" | grep -q "tenant 2: 10 packets" || { echo "FAIL tenant 2"; exit 1; }
echo "PASS: per-tenant counters correct"
