#!/usr/bin/env bash
# Build VPP with the tenantcount plugin dropped into src/plugins/.
# Usage: scripts/build.sh [vpp-tag]     (default: stable/2506 branch)
set -euo pipefail

HERE="$(cd "$(dirname "$0")/.." && pwd)"
VPP_REF="${1:-stable/2506}"
VPP_DIR="${VPP_DIR:-$HERE/build/vpp}"

mkdir -p "$HERE/build"
if [ ! -d "$VPP_DIR/.git" ]; then
  git clone --depth 1 --branch "$VPP_REF" https://github.com/FDio/vpp.git "$VPP_DIR"
fi

# Sync plugin sources into the VPP tree (re-run after every edit).
rm -rf "$VPP_DIR/src/plugins/tenantcount"
mkdir -p "$VPP_DIR/src/plugins/tenantcount"
cp "$HERE"/plugin/* "$VPP_DIR/src/plugins/tenantcount/"

cd "$VPP_DIR"
if [ ! -f .deps-installed ]; then
  make install-dep        # needs sudo; installs build dependencies
  touch .deps-installed
fi
make build                # debug build (fast iteration, asserts enabled)

echo
echo "Built. Binaries: $VPP_DIR/build-root/install-vpp_debug-native/vpp/bin/vpp"
