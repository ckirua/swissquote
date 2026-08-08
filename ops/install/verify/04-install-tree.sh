#!/usr/bin/env bash
set -eu
# shellcheck source=../common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../common.sh"

for path in cpp third_party/simdjson sql ops install.sh; do
  [[ -e "${INSTALL_ROOT}/${path}" ]] || { echo "missing ${INSTALL_ROOT}/${path}"; exit 1; }
done
echo "ok install-tree"
