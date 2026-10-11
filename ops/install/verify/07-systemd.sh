#!/usr/bin/env bash
set -eu
# shellcheck source=../common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../common.sh"

[[ -f "/etc/systemd/system/${FEED_UNIT}" ]] || exit 1
[[ -f "/etc/systemd/system/${WRITER_UNIT}" ]] || exit 1
[[ -f /etc/systemd/system/swissquote-bbo-archive.service ]] || exit 1
[[ -f /etc/systemd/system/swissquote-bbo-archive.timer ]] || exit 1
systemd-analyze verify "/etc/systemd/system/${FEED_UNIT}" "/etc/systemd/system/${WRITER_UNIT}" /etc/systemd/system/swissquote-bbo-archive.service /etc/systemd/system/swissquote-bbo-archive.timer
systemctl is-enabled "${FEED_UNIT}" >/dev/null
systemctl is-enabled "${WRITER_UNIT}" >/dev/null
systemctl is-enabled swissquote-bbo-archive.timer >/dev/null
systemctl is-active swissquote-bbo-archive.timer >/dev/null
systemctl show swissquote-bbo-archive.timer -p TimersCalendar --value | grep -F '*-*-* 06:00:00 UTC' >/dev/null
echo "ok systemd"
