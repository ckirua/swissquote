#!/usr/bin/env bash
set -eu
# shellcheck source=../common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
require_root

export INSTALL_ROOT CONFIG_PATH ENV_PATH SERVICE_USER FEED_UNIT UNIT_SUFFIX

render_unit() {
  local template="$1"
  local dest="$2"
  sed \
    -e "s|@INSTALL_ROOT@|${INSTALL_ROOT}|g" \
    -e "s|@CONFIG_PATH@|${CONFIG_PATH}|g" \
    -e "s|@ENV_PATH@|${ENV_PATH}|g" \
    -e "s|@SERVICE_USER@|${SERVICE_USER}|g" \
    -e "s|@FEED_UNIT@|${FEED_UNIT}|g" \
    -e "s|@UNIT_SUFFIX@|${UNIT_SUFFIX}|g" \
    <"${template}" >"${dest}.tmp"
  install -m 644 "${dest}.tmp" "${dest}"
  rm -f "${dest}.tmp"
}

render_unit "${INSTALL_ROOT}/systemd/swissquote-feed.service.in" "/etc/systemd/system/${FEED_UNIT}"
render_unit "${INSTALL_ROOT}/systemd/swissquote-writer.service.in" "/etc/systemd/system/${WRITER_UNIT}"
install -m 644 "${INSTALL_ROOT}/systemd/swissquote-bbo-archive.service" /etc/systemd/system/swissquote-bbo-archive.service
install -m 644 "${INSTALL_ROOT}/systemd/swissquote-bbo-archive.timer" /etc/systemd/system/swissquote-bbo-archive.timer

systemctl daemon-reload
systemctl enable "${FEED_UNIT}" "${WRITER_UNIT}"
systemctl enable --now swissquote-bbo-archive.timer

log "systemd units installed (${FEED_UNIT}, ${WRITER_UNIT}); daily archive timer enabled"
