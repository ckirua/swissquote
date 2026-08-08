#!/usr/bin/env bash
set -eu
# shellcheck source=../common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
require_root

if [[ -d "${INSTALL_ROOT}" && "${INSTALL_ROOT}" != "/" ]]; then
  if [[ -d "${INSTALL_ROOT}.prev" ]]; then
    rm -rf "${INSTALL_ROOT}.prev"
  fi
  if [[ -n "$(ls -A "${INSTALL_ROOT}" 2>/dev/null || true)" ]]; then
    mv "${INSTALL_ROOT}" "${INSTALL_ROOT}.prev"
    install -d -m 750 -o root -g "${SERVICE_USER}" "${INSTALL_ROOT}"
  fi
fi

rsync -a --delete \
  --exclude 'cpp/build/' \
  --exclude '.git/' \
  --exclude '__pycache__/' \
  --exclude '.venv/' \
  --exclude 'nbs/.ipynb_checkpoints/' \
  "${REPO_ROOT}/" "${INSTALL_ROOT}/"

if [[ -f "${REPO_ROOT}/cpp/VERSION" ]]; then
  install -m 644 "${REPO_ROOT}/cpp/VERSION" "${INSTALL_ROOT}/VERSION"
fi

chown -R root:"${SERVICE_USER}" "${INSTALL_ROOT}"
find "${INSTALL_ROOT}" -type d -exec chmod 750 {} +
find "${INSTALL_ROOT}" -type f -exec chmod 640 {} +
chmod 750 "${INSTALL_ROOT}/install.sh" 2>/dev/null || true
find "${INSTALL_ROOT}/ops" -name '*.sh' -exec chmod 750 {} + 2>/dev/null || true

log "release tree installed to ${INSTALL_ROOT}"
