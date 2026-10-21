#!/usr/bin/env bash
# Staging-only writer preflight/launcher. This script never manages system services.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WRITER="${REPO_ROOT}/cpp/build/swissquote-writer"
PRODUCTION_CONFIG="/etc/swissquote/service.toml"
LEGACY_PRODUCTION_CONFIG="/home/swissquote/.config/service.toml"
PRODUCTION_SOCKET_PATH="/run/swissquote/feed.sock"
STAGING_SOCKET_PATH="${STAGING_SOCKET_PATH:-/run/swissquote-staging/feed.sock}"
PRODUCTION_CONFIGS=("${PRODUCTION_CONFIG}" "${LEGACY_PRODUCTION_CONFIG}")

fail() {
  echo "ERROR: $*" >&2
  exit 1
}

: "${STAGING_CONFIG:?STAGING_CONFIG must be set to a staging service.toml}"

for production_config in "${PRODUCTION_CONFIGS[@]}"; do
  if [[ "${STAGING_CONFIG}" == "${production_config}" ]]; then
    fail "STAGING_CONFIG must not be a production config (${production_config})"
  fi
done

# Resolve paths so a symlink or relative path cannot select a production config.
config_real="$(realpath -m -- "${STAGING_CONFIG}")"
for production_config in "${PRODUCTION_CONFIGS[@]}"; do
  production_config_real="$(realpath -m -- "${production_config}")"
  if [[ "${config_real}" == "${production_config_real}" ]]; then
    fail "STAGING_CONFIG resolves to a production config (${production_config})"
  fi
done
if [[ ! -f "${STAGING_CONFIG}" ]]; then
  fail "staging config does not exist or is not a regular file: ${STAGING_CONFIG}"
fi

if [[ ! -x "${WRITER}" ]]; then
  fail "built writer binary is missing or not executable: ${WRITER}"
fi

if [[ "${STAGING_SOCKET_PATH}" == "${PRODUCTION_SOCKET_PATH}" ]]; then
  fail "STAGING_SOCKET_PATH must differ from the production socket (${PRODUCTION_SOCKET_PATH})"
fi
if [[ "${STAGING_SOCKET_PATH}" != /* || "${STAGING_SOCKET_PATH}" =~ [[:space:]] ]]; then
  fail "STAGING_SOCKET_PATH must be an absolute path without whitespace"
fi
if ! grep -Fqx "socket_path = \"${STAGING_SOCKET_PATH}\"" "${STAGING_CONFIG}"; then
  fail "staging config must select STAGING_SOCKET_PATH=${STAGING_SOCKET_PATH}"
fi

CMD=("${WRITER}" --config "${STAGING_CONFIG}")
printf 'Command:'
printf ' %q' "${CMD[@]}"
printf '\n'
printf 'staging SOCK_SEQPACKET socket: %s\n' "${STAGING_SOCKET_PATH}"
printf 'staging config: %s\n' "${STAGING_CONFIG}"


DRY_RUN="${DRY_RUN:-1}"
RUN_STAGING="${RUN_STAGING:-0}"
case "${DRY_RUN}" in
  0|1) ;;
  *) fail "DRY_RUN must be 0 or 1" ;;
esac
case "${RUN_STAGING}" in
  0|1) ;;
  *) fail "RUN_STAGING must be 0 or 1" ;;
esac
if [[ "${RUN_STAGING}" != "1" || "${DRY_RUN}" == "1" ]]; then
  echo "Preflight only; writer not started (set RUN_STAGING=1 and DRY_RUN=0 to execute)."
  exit 0
fi

WRITER_PID=""
cleanup() {
  local status=$?
  trap - EXIT INT TERM
  if [[ -n "${WRITER_PID:-}" ]] && kill -0 "${WRITER_PID}" 2>/dev/null; then
    kill "${WRITER_PID}" 2>/dev/null || true
    wait "${WRITER_PID}" 2>/dev/null || true
  fi
  exit "${status}"
}
trap cleanup EXIT INT TERM

"${CMD[@]}" &
WRITER_PID=$!
set +e
wait "${WRITER_PID}"
status=$?
set -e
WRITER_PID=""
exit "${status}"
