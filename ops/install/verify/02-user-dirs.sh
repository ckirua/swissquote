#!/usr/bin/env bash
set -eu
# shellcheck source=../common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../common.sh"

id "${SERVICE_USER}" >/dev/null
for dir in "${INSTALL_ROOT}" "${CONFIG_DIR}" "${STATE_DIR}" "${LOG_DIR}"; do
  [[ -d "${dir}" ]] || { echo "missing ${dir}"; exit 1; }
  mode="$(stat -c '%a' "${dir}")"
  [[ "${mode}" == "750" ]] || { echo "bad mode ${dir}: ${mode}"; exit 1; }
done
echo "ok user-dirs"
