#!/usr/bin/env bash
# Start the freshly built debug VPP with our startup.conf (needs root).
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
VPP_DIR="${VPP_DIR:-$HERE/build/vpp}"
ROOT="$VPP_DIR/build-root/install-vpp_debug-native/vpp"
sudo mkdir -p /run/vpp
exec sudo "$ROOT/bin/vpp" -c "$HERE/scripts/startup.conf"
