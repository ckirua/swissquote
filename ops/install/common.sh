#!/usr/bin/env bash
# Shared variables and helpers for gated prod install epochs.
set -eu

INSTALL_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${INSTALL_SCRIPT_DIR}/../.." && pwd)"

SERVICE_USER="${SERVICE_USER:-swissquote}"
STAGING="${STAGING:-0}"
DRY_RUN="${DRY_RUN:-0}"
FORCE="${FORCE:-0}"
SKIP_CLICKHOUSE="${SKIP_CLICKHOUSE:-0}"
WITH_LOCAL_CLICKHOUSE="${WITH_LOCAL_CLICKHOUSE:-0}"
FROM_EPOCH="${FROM_EPOCH:-0}"
ONLY_EPOCH="${ONLY_EPOCH:-}"
ENV_FILE_INPUT="${ENV_FILE_INPUT:-}"

if [[ "${STAGING}" == "1" ]]; then
  INSTALL_ROOT="${INSTALL_ROOT:-/opt/swissquote-staging}"
  CONFIG_DIR="${CONFIG_DIR:-/etc/swissquote-staging}"
  STATE_DIR="${STATE_DIR:-/var/lib/swissquote-staging}"
  LOG_DIR="${LOG_DIR:-/var/log/swissquote-staging}"
  RUN_DIRECTORY="${RUN_DIRECTORY:-swissquote-staging}"
  SOCKET_PATH="${SOCKET_PATH:-/run/swissquote-staging/feed.sock}"
  UNIT_SUFFIX="${UNIT_SUFFIX:--staging}"
else
  INSTALL_ROOT="${INSTALL_ROOT:-/opt/swissquote}"
  CONFIG_DIR="${CONFIG_DIR:-/etc/swissquote}"
  STATE_DIR="${STATE_DIR:-/var/lib/swissquote}"
  LOG_DIR="${LOG_DIR:-/var/log/swissquote}"
  RUN_DIRECTORY="${RUN_DIRECTORY:-swissquote}"
  SOCKET_PATH="${SOCKET_PATH:-/run/swissquote/feed.sock}"
  UNIT_SUFFIX="${UNIT_SUFFIX:-}"
fi

INSTALL_PREFIX="${INSTALL_PREFIX:-${INSTALL_ROOT}}"
CONFIG_PATH="${CONFIG_PATH:-${CONFIG_DIR}/service.toml}"
ENV_PATH="${ENV_PATH:-${CONFIG_DIR}/env}"
STATE_FILE="${STATE_FILE:-${STATE_DIR}/install.state}"
FEED_UNIT="swissquote-feed${UNIT_SUFFIX}.service"
WRITER_UNIT="swissquote-writer${UNIT_SUFFIX}.service"
FEED_BIN="${INSTALL_PREFIX}/bin/swissquote-feed"
WRITER_BIN="${INSTALL_PREFIX}/bin/swissquote-writer"

DEPLOY_MODE=prod
export DEPLOY_MODE SERVICE_USER INSTALL_ROOT INSTALL_PREFIX CONFIG_DIR CONFIG_PATH ENV_PATH LOG_DIR STATE_DIR STATE_FILE SOCKET_PATH RUN_DIRECTORY
export REPO_ROOT STAGING DRY_RUN FORCE SKIP_CLICKHOUSE WITH_LOCAL_CLICKHOUSE UNIT_SUFFIX FEED_UNIT WRITER_UNIT

die() {
  echo "ERROR: $*" >&2
  exit 1
}

log() {
  echo "==> $*"
}

require_root() {
  if [[ "${EUID}" -ne 0 ]]; then
    die "requires root (sudo)"
  fi
}

epoch_tag() {
  printf '%02d' "$1"
}

epoch_exit_code() {
  echo $((10 + $1))
}

epoch_script_path() {
  local id="$1"
  local slug="$2"
  echo "${INSTALL_SCRIPT_DIR}/$(epoch_tag "${id}")-${slug}.sh"
}

write_state() {
  local epoch="$1"
  mkdir -p "$(dirname "${STATE_FILE}")"
  cat >"${STATE_FILE}" <<EOF
LAST_PASSED_EPOCH=${epoch}
INSTALL_ROOT=${INSTALL_ROOT}
CONFIG_DIR=${CONFIG_DIR}
STAGING=${STAGING}
UPDATED_AT=$(date -u +%Y-%m-%dT%H:%M:%SZ)
EOF
  chmod 640 "${STATE_FILE}" 2>/dev/null || true
}

should_run_epoch() {
  local id="$1"
  if [[ -n "${ONLY_EPOCH}" ]]; then
    [[ "${ONLY_EPOCH}" == "${id}" ]]
    return
  fi
  [[ "${id}" -ge "${FROM_EPOCH}" ]]
}

run_epoch() {
  local id="$1"
  local slug="$2"
  local title="$3"
  local script verify

  script="$(epoch_script_path "${id}" "${slug}")"
  verify="${INSTALL_SCRIPT_DIR}/verify/$(epoch_tag "${id}")-${slug}.sh"

  if ! should_run_epoch "${id}"; then
    return 0
  fi

  log "Epoch ${id}: ${title}"
  if [[ ! -f "${script}" ]]; then
    die "missing epoch script ${script}"
  fi
  if [[ "${DRY_RUN}" == "1" && "${id}" -gt 0 ]]; then
    echo "[dry-run] would run ${script} && ${verify}"
    return 0
  fi

  if ! bash "${script}"; then
    echo "FAIL epoch ${id}: ${title} (exit $(epoch_exit_code "${id}"))" >&2
    exit "$(epoch_exit_code "${id}")"
  fi
  if [[ ! -f "${verify}" ]]; then
    die "missing verify script ${verify}"
  fi
  if ! bash "${verify}"; then
    echo "FAIL verify epoch ${id}: ${title} (exit $(epoch_exit_code "${id}"))" >&2
    exit "$(epoch_exit_code "${id}")"
  fi

  if [[ "${DRY_RUN}" != "1" ]]; then
    write_state "${id}"
  fi
  echo "PASS epoch ${id}: ${title}"
}

print_plan() {
  cat <<EOF
Install plan ($(if [[ "${STAGING}" == 1 ]]; then echo staging; else echo prod; fi))
  REPO_ROOT=${REPO_ROOT}
  INSTALL_ROOT=${INSTALL_ROOT}
  CONFIG_DIR=${CONFIG_DIR}
  LOG_DIR=${LOG_DIR}
  STATE_FILE=${STATE_FILE}
  FEED_UNIT=${FEED_UNIT}
  WRITER_UNIT=${WRITER_UNIT}
  FROM_EPOCH=${FROM_EPOCH}
  ONLY_EPOCH=${ONLY_EPOCH:-all}
  SKIP_CLICKHOUSE=${SKIP_CLICKHOUSE}
  WITH_LOCAL_CLICKHOUSE=${WITH_LOCAL_CLICKHOUSE}
EOF
}
