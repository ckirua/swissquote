#!/usr/bin/env bash
set -eu
# shellcheck source=../common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
require_root

export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y --no-install-recommends \
  cmake \
  g++ \
  libssl-dev \
  curl \
  ca-certificates \
  python3 \
  gettext-base \
  rsync

log "packages installed"
