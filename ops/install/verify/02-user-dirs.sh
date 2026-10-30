#!/usr/bin/env bash
set -eu
# shellcheck source=../common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../common.sh"

source "${REPO_ROOT}/ops/_permissions.sh"

id "${SERVICE_USER}" >/dev/null
PERMISSIONS_OK=true
check_private_dir "install directory" "${INSTALL_ROOT}" root 750 "${SERVICE_USER}"
check_private_dir "state directory" "${STATE_DIR}" root 750 "${SERVICE_USER}"
check_production_config_dirs
[[ "${PERMISSIONS_OK}" == true ]] || exit 1
echo "ok user-dirs"
