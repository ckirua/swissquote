#!/usr/bin/env bash
# Preflight checks (no root). Enforces file permissions before deploy.
# Run: bash ops/deploy-check.sh
# Prod: DEPLOY_MODE=prod bash ops/deploy-check.sh
set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${REPO_ROOT}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=_permissions.sh
source "${SCRIPT_DIR}/_permissions.sh"

DEPLOY_MODE="${DEPLOY_MODE:-dev}"
resolve_deploy_paths

ok=true
PERMISSIONS_OK=true

check() {
  if eval "$2"; then
    echo "ok  $1"
  else
    echo "FAIL $1"
    ok=false
  fi
}

if [[ "${AUTO_FIX:-1}" != "0" ]]; then
  echo "==> Auto-fix permissions (set AUTO_FIX=0 to skip)"
  apply_secure_permissions || true
fi

echo "==> Mode: ${DEPLOY_MODE}"
echo "==> Build deps"
check "cmake" "command -v cmake >/dev/null"
check "g++ or clang++" "command -v g++ >/dev/null || command -v clang++ >/dev/null"
check "libssl (HTTPS feed)" "[[ -f /usr/include/openssl/ssl.h ]]"
check "python3 (IPC migration and smoke)" "command -v python3 >/dev/null"

echo "==> Paths"
check "feed binary" "[[ -x '${FEED_BIN}' ]]"
check "writer binary" "[[ -x '${WRITER_BIN}' ]]"
check "systemd feed unit" "[[ -f '${SYSTEMD_FEED_UNIT}' ]]"
check "systemd writer unit" "[[ -f '${SYSTEMD_WRITER_UNIT}' ]]"

echo "==> Permissions"
if [[ "${DEPLOY_MODE}" == "prod" ]]; then
  check_private_dir "config directory" "${CONFIG_DIR}" "root"
  check_private_dir "log directory" "${LOG_DIR}" "root"
else
  check_private_dir "config directory" "${CONFIG_DIR}" "${SERVICE_USER}"
  check_private_dir "log directory" "${LOG_DIR}" "${SERVICE_USER}"
fi
check_private_file "env file" "${ENV_FILE}" "${SERVICE_USER}"
check_private_file "service config" "${CONFIG}" "${SERVICE_USER}"
check_clickhouse_secrets_in_config "${CONFIG}"

if [[ -f "${ENV_FILE}" ]]; then
  # shellcheck disable=SC1090
  set -a
  source "${ENV_FILE}"
  set +a
  check "CLICKHOUSE_HOST in env" "[[ -n '${CLICKHOUSE_HOST:-}' ]]"
else
  check "CLICKHOUSE_HOST in env" "false"
fi

if [[ "${PERMISSIONS_OK}" != true ]]; then
  ok=false
fi

if [[ "${ok}" == true ]]; then
  echo
  if [[ "${DEPLOY_MODE}" == "prod" ]]; then
    echo "Preflight passed (prod). Services:"
    echo "  systemctl status swissquote-feed swissquote-writer"
  else
    echo "Preflight passed (dev). Install with:"
    echo "  sudo bash ops/deploy.sh"
  fi
  exit 0
fi

echo
echo "Preflight failed. Fix permissions with:"
echo "  bash ops/secure_permissions.sh"
exit 1
