#!/usr/bin/env bash
set -eu
# shellcheck source=../common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
require_root

if ! id "${SERVICE_USER}" >/dev/null 2>&1; then
  useradd -m -s /bin/bash "${SERVICE_USER}"
  log "created user ${SERVICE_USER}"
fi

for dir in "${INSTALL_ROOT}" "${CONFIG_DIR}" "${STATE_DIR}"; do
  install -d -m 750 -o root -g "${SERVICE_USER}" "${dir}"
done
install -d -m 750 -o "${SERVICE_USER}" -g "${SERVICE_USER}" "${LOG_DIR}"

log "directories ready"
