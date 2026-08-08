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
  pkg-config \
  libzmq3-dev \
  libssl-dev \
  curl \
  ca-certificates \
  gettext-base \
  rsync

log "packages installed"
