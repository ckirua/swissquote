#!/usr/bin/env bash
# Shared permission checks for deploy-check.sh and deploy.sh.
# Source: source "$(dirname "${BASH_SOURCE[0]}")/_permissions.sh"

resolve_service_home() {
  SERVICE_USER="${SERVICE_USER:-swissquote}"
  if [[ -z "${HOME_DIR:-}" ]]; then
    if [[ "${EUID}" -eq 0 ]] && command -v getent >/dev/null; then
      HOME_DIR="$(getent passwd "${SERVICE_USER}" | cut -d: -f6 || true)"
    else
      HOME_DIR="${HOME}"
    fi
  fi
  if [[ -z "${HOME_DIR}" || ! -d "${HOME_DIR}" ]]; then
    echo "error: home directory for ${SERVICE_USER} not found" >&2
    return 1
  fi
}

resolve_deploy_paths() {
  DEPLOY_MODE="${DEPLOY_MODE:-dev}"
  resolve_service_home || return 1

  if [[ "${DEPLOY_MODE}" == "prod" ]]; then
    INSTALL_PREFIX="${INSTALL_PREFIX:-${INSTALL_ROOT:-/opt/swissquote}}"
    CONFIG_DIR="${CONFIG_DIR:-/etc/swissquote}"
    CONFIG="${CONFIG:-${CONFIG_DIR}/service.toml}"
    ENV_FILE="${ENV_FILE:-${CONFIG_DIR}/env}"
    LOG_DIR="${LOG_DIR:-/var/log/swissquote}"
    FEED_BIN="${INSTALL_PREFIX}/bin/swissquote-feed"
    WRITER_BIN="${INSTALL_PREFIX}/bin/swissquote-writer"
    SYSTEMD_FEED_UNIT="${SYSTEMD_FEED_UNIT:-/etc/systemd/system/swissquote-feed.service}"
    SYSTEMD_WRITER_UNIT="${SYSTEMD_WRITER_UNIT:-/etc/systemd/system/swissquote-writer.service}"
  else
    INSTALL_PREFIX="${INSTALL_PREFIX:-${HOME_DIR}/.local}"
    CONFIG_DIR="${HOME_DIR}/.config"
    CONFIG="${CONFIG_DIR}/service.toml"
    ENV_FILE="${HOME_DIR}/.env"
    LOG_DIR="${HOME_DIR}/.local/log"
    FEED_BIN="${INSTALL_PREFIX}/bin/swissquote-feed"
    WRITER_BIN="${INSTALL_PREFIX}/bin/swissquote-writer"
    SYSTEMD_FEED_UNIT="${REPO_ROOT:-.}/systemd/dev/swissquote-feed.service"
    SYSTEMD_WRITER_UNIT="${REPO_ROOT:-.}/systemd/dev/swissquote-writer.service"
  fi
  export INSTALL_PREFIX CONFIG CONFIG_DIR ENV_FILE LOG_DIR FEED_BIN WRITER_BIN
  export SYSTEMD_FEED_UNIT SYSTEMD_WRITER_UNIT
}

check_private_file() {
  local label="$1"
  local path="$2"
  local expected_owner="${3:-${SERVICE_USER}}"

  if [[ ! -f "${path}" ]]; then
    echo "FAIL ${label} (missing ${path})"
    PERMISSIONS_OK=false
    return
  fi

  local mode owner
  mode="$(stat -c '%a' "${path}")"
  owner="$(stat -c '%U' "${path}")"

  if [[ "${mode}" == "600" || "${mode}" == "400" ]]; then
    echo "ok  ${label} (mode ${mode})"
  else
    echo "FAIL ${label} (mode ${mode}, expected 600)"
    PERMISSIONS_OK=false
  fi

  if [[ "${owner}" != "${expected_owner}" ]]; then
    echo "FAIL ${label} owner (got ${owner}, expected ${expected_owner})"
    PERMISSIONS_OK=false
  else
    echo "ok  ${label} owner (${owner})"
  fi
}

check_private_dir() {
  local label="$1"
  local path="$2"
  local expected_owner="${3:-${SERVICE_USER}}"

  if [[ ! -d "${path}" ]]; then
    echo "FAIL ${label} (missing ${path})"
    PERMISSIONS_OK=false
    return
  fi

  local mode owner
  mode="$(stat -c '%a' "${path}")"
  owner="$(stat -c '%U' "${path}")"

  if [[ "${mode}" == "700" ]]; then
    echo "ok  ${label} (mode ${mode})"
  else
    echo "FAIL ${label} (mode ${mode}, expected 700)"
    PERMISSIONS_OK=false
  fi

  if [[ "${owner}" != "${expected_owner}" ]]; then
    echo "FAIL ${label} owner (got ${owner}, expected ${expected_owner})"
    PERMISSIONS_OK=false
  else
    echo "ok  ${label} owner (${owner})"
  fi
}

check_clickhouse_secrets_in_config() {
  local config="$1"
  if [[ ! -f "${config}" ]]; then
    echo "FAIL service config (missing ${config})"
    PERMISSIONS_OK=false
    return
  fi
  if awk '
    /^\[clickhouse\]/ { in_ch=1; next }
    /^\[/ { in_ch=0 }
    in_ch && /^[[:space:]]*(host|password)[[:space:]]*=/ { found=1 }
    END { exit !found }
  ' "${config}"; then
    echo "FAIL service.toml [clickhouse] must not set host/password (use env file)"
    PERMISSIONS_OK=false
  else
    echo "ok  service.toml has no clickhouse secrets"
  fi
}

apply_secure_permissions() {
  if [[ "${DEPLOY_MODE:-dev}" == "prod" ]]; then
    resolve_deploy_paths || return 1
    mkdir -p "${CONFIG_DIR}" "${LOG_DIR}"
    if [[ "${EUID}" -eq 0 ]]; then
      install -d -m 750 -o root -g "${SERVICE_USER}" "${CONFIG_DIR}" "${LOG_DIR}"
      [[ -f "${ENV_FILE}" ]] && chown "${SERVICE_USER}:${SERVICE_USER}" "${ENV_FILE}" && chmod 600 "${ENV_FILE}"
      [[ -f "${CONFIG}" ]] && chown "${SERVICE_USER}:${SERVICE_USER}" "${CONFIG}" && chmod 600 "${CONFIG}"
    else
      chmod 750 "${CONFIG_DIR}" "${LOG_DIR}" 2>/dev/null || true
      [[ -f "${ENV_FILE}" ]] && chmod 600 "${ENV_FILE}"
      [[ -f "${CONFIG}" ]] && chmod 600 "${CONFIG}"
    fi
    return 0
  fi

  local env_file="${HOME_DIR}/.env"
  local config="${HOME_DIR}/.config/service.toml"
  local config_dir="${HOME_DIR}/.config"
  local log_dir="${HOME_DIR}/.local/log"

  mkdir -p "${config_dir}" "${log_dir}"

  if [[ "${EUID}" -eq 0 ]]; then
    install -d -o "${SERVICE_USER}" -g "${SERVICE_USER}" -m 700 "${config_dir}" "${log_dir}"
    [[ -f "${env_file}" ]] && chown "${SERVICE_USER}:${SERVICE_USER}" "${env_file}" && chmod 600 "${env_file}"
    [[ -f "${config}" ]] && chown "${SERVICE_USER}:${SERVICE_USER}" "${config}" && chmod 600 "${config}"
    chown "${SERVICE_USER}:${SERVICE_USER}" "${config_dir}" "${log_dir}"
  else
    chmod 700 "${config_dir}" "${log_dir}" 2>/dev/null || true
    [[ -f "${env_file}" ]] && chmod 600 "${env_file}"
    [[ -f "${config}" ]] && chmod 600 "${config}"
  fi
}
