#!/usr/bin/env bash
# Set permissions for the selected dev/prod deployment layout.
# Dev: bash ops/secure_permissions.sh
# Prod: sudo DEPLOY_MODE=prod bash ops/secure_permissions.sh
set -eu

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=_permissions.sh
source "${SCRIPT_DIR}/_permissions.sh"

resolve_deploy_paths
apply_secure_permissions

echo "permissions set:"
for path in "${CONFIG_DIR}" "${LOG_DIR}" "${ENV_FILE}" "${CONFIG}"; do
  if [[ -e "${path}" ]]; then
    stat -c '%a %U:%G %n' "${path}"
  fi
done
