#!/usr/bin/env bash
# Stop prod/staging units and optionally restore previous /opt snapshot.
set -eu

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGING=0
RESTORE=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --staging) STAGING=1; shift ;;
    --restore) RESTORE=1; shift ;;
    -h | --help)
      echo "Usage: sudo bash ops/install/rollback.sh [--staging] [--restore]"
      exit 0
      ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
  esac
done

export STAGING
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"
require_root

systemctl stop "${WRITER_UNIT}" 2>/dev/null || true
systemctl stop "${FEED_UNIT}" 2>/dev/null || true
systemctl disable "${WRITER_UNIT}" 2>/dev/null || true
systemctl disable "${FEED_UNIT}" 2>/dev/null || true
rm -f "/etc/systemd/system/${FEED_UNIT}" "/etc/systemd/system/${WRITER_UNIT}"
systemctl daemon-reload

if [[ "${RESTORE}" == "1" && -d "${INSTALL_ROOT}.prev" ]]; then
  rm -rf "${INSTALL_ROOT}"
  mv "${INSTALL_ROOT}.prev" "${INSTALL_ROOT}"
  echo "restored ${INSTALL_ROOT} from ${INSTALL_ROOT}.prev"
fi

echo "rollback complete"
