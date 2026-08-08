#!/usr/bin/env bash
# Tighten permissions on user-local config and secrets.
# Run: bash ops/secure_permissions.sh
#      sudo HOME_DIR=/home/swissquote SERVICE_USER=swissquote bash ops/secure_permissions.sh
set -eu

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=_permissions.sh
source "${SCRIPT_DIR}/_permissions.sh"

resolve_service_home
apply_secure_permissions

echo "permissions set:"
stat -c '%a %U %n' "${HOME_DIR}/.config" "${HOME_DIR}/.local/log"
[[ -f "${HOME_DIR}/.env" ]] && stat -c '%a %U %n' "${HOME_DIR}/.env"
[[ -f "${HOME_DIR}/.config/service.toml" ]] && stat -c '%a %U %n' "${HOME_DIR}/.config/service.toml"
