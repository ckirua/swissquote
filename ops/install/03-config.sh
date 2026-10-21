#!/usr/bin/env bash
set -eu
# shellcheck source=../common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
require_root

TEMPLATE="${REPO_ROOT}/python/config.example.toml"
[[ -f "${TEMPLATE}" ]] || die "missing ${TEMPLATE}"
sed \
  -e "s|/home/swissquote/.local/log|${LOG_DIR}|g" \
  -e "s|/home/swissquote/.config/service.toml|${CONFIG_PATH}|g" \
  -e "s|socket_path = \"/run/swissquote/feed.sock\"|socket_path = \"${SOCKET_PATH}\"|g" \
  "${TEMPLATE}" >"${CONFIG_PATH}.tmp"
install -m 600 -o "${SERVICE_USER}" -g "${SERVICE_USER}" "${CONFIG_PATH}.tmp" "${CONFIG_PATH}"
rm -f "${CONFIG_PATH}.tmp"

if [[ -n "${ENV_FILE_INPUT}" && -f "${ENV_FILE_INPUT}" ]]; then
  install -m 600 -o "${SERVICE_USER}" -g "${SERVICE_USER}" "${ENV_FILE_INPUT}" "${ENV_PATH}"
elif [[ -f "${ENV_PATH}" ]]; then
  log "keeping existing ${ENV_PATH}"
else
  host="${CLICKHOUSE_HOST:-}"
  if [[ -z "${host}" ]]; then
    read -r -p "CLICKHOUSE_HOST: " host
  fi
  pass="${CLICKHOUSE_PASSWORD:-}"
  cat >"${ENV_PATH}.tmp" <<EOF
CLICKHOUSE_HOST=${host}
CLICKHOUSE_HTTP_PORT=${CLICKHOUSE_HTTP_PORT:-8123}
CLICKHOUSE_PORT=${CLICKHOUSE_PORT:-9000}
CLICKHOUSE_USER=${CLICKHOUSE_USER:-default}
CLICKHOUSE_PASSWORD=${pass}
EOF
  install -m 600 -o "${SERVICE_USER}" -g "${SERVICE_USER}" "${ENV_PATH}.tmp" "${ENV_PATH}"
  rm -f "${ENV_PATH}.tmp"
fi

log "config written"
