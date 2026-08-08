#!/usr/bin/env bash
# Staging-only writer preflight/launcher. This script never manages system services.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WRITER="${REPO_ROOT}/cpp/build/swissquote-writer"
PRODUCTION_CONFIG="/home/swissquote/.config/service.toml"
PRODUCTION_ENDPOINT="tcp://127.0.0.1:5559"

fail() {
  echo "ERROR: $*" >&2
  exit 1
}

: "${STAGING_CONFIG:?STAGING_CONFIG must be set to a staging service.toml}"
: "${STAGING_ZMQ_ENDPOINT:?STAGING_ZMQ_ENDPOINT must be set to a staging tcp endpoint}"

if [[ "${STAGING_CONFIG}" == "${PRODUCTION_CONFIG}" ]]; then
  fail "STAGING_CONFIG must not be the production config (${PRODUCTION_CONFIG})"
fi

# Resolve both paths so a symlink or relative path cannot select production config.
config_real="$(realpath -m -- "${STAGING_CONFIG}")"
production_config_real="$(realpath -m -- "${PRODUCTION_CONFIG}")"
if [[ "${config_real}" == "${production_config_real}" ]]; then
  fail "STAGING_CONFIG resolves to the production config (${PRODUCTION_CONFIG})"
fi
if [[ ! -f "${STAGING_CONFIG}" ]]; then
  fail "staging config does not exist or is not a regular file: ${STAGING_CONFIG}"
fi

if [[ ! -x "${WRITER}" ]]; then
  fail "built writer binary is missing or not executable: ${WRITER}"
fi

if [[ "${STAGING_ZMQ_ENDPOINT}" == "${PRODUCTION_ENDPOINT}" ]]; then
  fail "STAGING_ZMQ_ENDPOINT must differ from the production endpoint (${PRODUCTION_ENDPOINT})"
fi

# swissquote-writer accepts the host and port separately, so validate and split the
# required endpoint before constructing its argv. Keep the accepted form explicit.
if [[ ! "${STAGING_ZMQ_ENDPOINT}" =~ ^tcp://([^[:space:]:]+):([0-9]+)$ ]]; then
  fail "STAGING_ZMQ_ENDPOINT must be a tcp://host:port endpoint"
fi
ZMQ_HOST="${BASH_REMATCH[1]}"
ZMQ_PORT="${BASH_REMATCH[2]}"
if ((10#${ZMQ_PORT} < 1 || 10#${ZMQ_PORT} > 65535)); then
  fail "STAGING_ZMQ_ENDPOINT port must be between 1 and 65535"
fi
if [[ "${ZMQ_HOST}" == "127.0.0.1" && $((10#${ZMQ_PORT})) -eq 5559 ]]; then
  fail "STAGING_ZMQ_ENDPOINT selects the production endpoint (${PRODUCTION_ENDPOINT})"
fi

CMD=("${WRITER}" --config "${STAGING_CONFIG}" --host "${ZMQ_HOST}" --port "${ZMQ_PORT}")
printf 'Command:'
printf ' %q' "${CMD[@]}"
printf '\n'
printf 'staging endpoint: %s\n' "${STAGING_ZMQ_ENDPOINT}"
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
