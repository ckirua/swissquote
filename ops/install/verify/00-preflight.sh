#!/usr/bin/env bash
set -eu
# shellcheck source=../common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../common.sh"

[[ -f /etc/os-release ]] || exit 1
# shellcheck disable=SC1091
source /etc/os-release
[[ "${ID}" == "ubuntu" || "${ID}" == "debian" ]] || exit 1
command -v curl >/dev/null || exit 1
echo "ok preflight"
