#!/usr/bin/env bash
set -eu
# shellcheck source=../common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
require_root

systemctl daemon-reload
systemctl enable "${FEED_UNIT}"
systemctl restart "${FEED_UNIT}"
sleep 2
systemctl enable "${WRITER_UNIT}"
systemctl restart "${WRITER_UNIT}"
sleep 2

log "services started"
