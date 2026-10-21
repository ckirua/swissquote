#!/usr/bin/env bash
# Install Debian/Ubuntu build dependencies for the Swissquote C++ engine.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ "${1:-}" == "--check" ]]; then
    "${SCRIPT_DIR}/check_toolchain.sh"
    exit 0
fi

if [[ $# -ne 0 ]]; then
    echo "usage: $0 [--check]" >&2
    exit 2
fi

if ! command -v apt-get >/dev/null 2>&1; then
    echo "ERROR: this helper supports Debian/Ubuntu systems with apt-get" >&2
    exit 1
fi

if [[ "${EUID}" -eq 0 ]]; then
    APT=(apt-get)
else
    if ! command -v sudo >/dev/null 2>&1; then
        echo "ERROR: sudo is required when not running as root" >&2
        exit 1
    fi
    APT=(sudo apt-get)
fi

export DEBIAN_FRONTEND=noninteractive
"${APT[@]}" update
"${APT[@]}" install -y --no-install-recommends \
    cmake \
    g++ \
    libssl-dev \
    curl \
    ca-certificates \
    gettext-base \
    python3 \
    rsync \
    default-jre-headless

"${SCRIPT_DIR}/check_toolchain.sh"
echo "C++ build dependencies are ready."
