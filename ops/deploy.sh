#!/usr/bin/env bash
# Install systemd units for the swissquote user layout (DEPLOY_MODE=dev).
# Prod installs use install.sh at repo root (see README).
set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SERVICE_USER="${SERVICE_USER:-swissquote}"
HOME_DIR="$(getent passwd "${SERVICE_USER}" | cut -d: -f6)"
INSTALL_PREFIX="${INSTALL_PREFIX:-${HOME_DIR}/.local}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=_permissions.sh
source "${SCRIPT_DIR}/_permissions.sh"

die() {
  echo "ERROR: $*" >&2
  exit 1
}

restart_unit() {
  local unit="$1"
  if systemctl restart "${unit}"; then
    echo "ok  restarted ${unit}"
  else
    echo "WARN ${unit} restart failed (service may have exited — check journalctl)" >&2
  fi
}

if [[ "${EUID}" -ne 0 ]]; then
  die "run as root: sudo bash ops/deploy.sh"
fi

if [[ -z "${HOME_DIR}" || ! -d "${HOME_DIR}" ]]; then
  die "user ${SERVICE_USER} not found"
fi

CONFIG="${HOME_DIR}/.config/service.toml"
ENV_FILE="${HOME_DIR}/.env"

for path in "${CONFIG}" "${ENV_FILE}"; do
  if [[ ! -e "${path}" ]]; then
    die "missing ${path}"
  fi
done

echo "==> Stopping legacy Python services (if running)"
systemctl stop swissquote-feed.service swissquote-writer.service 2>/dev/null || true
pkill -f 'scripts/examples/emitter.py' 2>/dev/null || true
pkill -f 'scripts/examples/writer.py' 2>/dev/null || true
sleep 1

echo "==> Building and installing C++ engine to ${INSTALL_PREFIX}"
cmake -S "${REPO_ROOT}/cpp" -B "${REPO_ROOT}/cpp/build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}"
cmake --build "${REPO_ROOT}/cpp/build" -j"$(nproc)"
cmake --install "${REPO_ROOT}/cpp/build"

FEED_BIN="${INSTALL_PREFIX}/bin/swissquote-feed"
WRITER_BIN="${INSTALL_PREFIX}/bin/swissquote-writer"
for path in "${FEED_BIN}" "${WRITER_BIN}"; do
  if [[ ! -x "${path}" ]]; then
    die "missing installed binary ${path}"
  fi
done
chown "${SERVICE_USER}:${SERVICE_USER}" "${FEED_BIN}" "${WRITER_BIN}"

echo "==> Securing permissions"
apply_secure_permissions

echo "==> Preflight (permissions enforced)"
export HOME_DIR SERVICE_USER INSTALL_PREFIX REPO_ROOT
if ! DEPLOY_MODE=dev AUTO_FIX=0 bash "${REPO_ROOT}/ops/deploy-check.sh"; then
  die "deploy-check failed after securing permissions"
fi

echo "==> Installing systemd units (dev)"
install -m 644 "${REPO_ROOT}/systemd/dev/swissquote-feed.service" /etc/systemd/system/
install -m 644 "${REPO_ROOT}/systemd/dev/swissquote-writer.service" /etc/systemd/system/
systemctl daemon-reload
systemctl enable swissquote-feed.service swissquote-writer.service

restart_unit swissquote-feed.service
restart_unit swissquote-writer.service

echo "==> Status"
systemctl --no-pager status swissquote-feed.service || true
echo
systemctl --no-pager status swissquote-writer.service || true

feed_ok=0
writer_ok=0
systemctl is-active --quiet swissquote-feed.service && feed_ok=1
systemctl is-active --quiet swissquote-writer.service && writer_ok=1

echo
if [[ "${feed_ok}" -eq 1 && "${writer_ok}" -eq 1 ]]; then
  echo "Deploy complete: both services are active."
elif [[ "${feed_ok}" -eq 1 ]]; then
  echo "Deploy complete: units installed, feed active, writer not running."
  echo "  sudo journalctl -u swissquote-writer.service -n 30 --no-pager"
elif [[ "${writer_ok}" -eq 1 ]]; then
  echo "Deploy complete: units installed, writer active, feed not running."
  echo "  sudo journalctl -u swissquote-feed.service -n 30 --no-pager"
else
  echo "Deploy complete: units installed, but services are not active."
  echo "  sudo journalctl -u swissquote-feed.service -n 30 --no-pager"
  echo "  sudo journalctl -u swissquote-writer.service -n 30 --no-pager"
fi

exit 0
