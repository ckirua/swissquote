#!/usr/bin/env bash
set -eu
# shellcheck source=../common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../common.sh"

systemctl is-active --quiet "${FEED_UNIT}" || exit 1
systemctl is-active --quiet "${WRITER_UNIT}" || exit 1
echo "ok start"
