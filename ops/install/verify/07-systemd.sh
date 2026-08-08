#!/usr/bin/env bash
set -eu
# shellcheck source=../common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../common.sh"

[[ -f "/etc/systemd/system/${FEED_UNIT}" ]] || exit 1
[[ -f "/etc/systemd/system/${WRITER_UNIT}" ]] || exit 1
systemd-analyze verify "/etc/systemd/system/${FEED_UNIT}" "/etc/systemd/system/${WRITER_UNIT}"
systemctl is-enabled "${FEED_UNIT}" >/dev/null
systemctl is-enabled "${WRITER_UNIT}" >/dev/null
echo "ok systemd"
