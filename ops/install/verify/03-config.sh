#!/usr/bin/env bash
set -eu
# shellcheck source=../common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../common.sh"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=../../_permissions.sh
source "${SCRIPT_DIR}/../_permissions.sh"

PERMISSIONS_OK=true
[[ -f "${CONFIG_PATH}" ]] || exit 1
[[ -f "${ENV_PATH}" ]] || exit 1
check_private_file "env" "${ENV_PATH}" "${SERVICE_USER}"
check_private_file "service config" "${CONFIG_PATH}" "${SERVICE_USER}"
check_clickhouse_secrets_in_config "${CONFIG_PATH}"
[[ "${PERMISSIONS_OK}" == true ]] || exit 1
# shellcheck disable=SC1090
set -a
source "${ENV_PATH}"
set +a
[[ -n "${CLICKHOUSE_HOST:-}" ]] || exit 1
echo "ok config"
