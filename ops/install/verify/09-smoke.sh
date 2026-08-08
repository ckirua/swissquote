#!/usr/bin/env bash
set -eu
# shellcheck source=../common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../common.sh"

bash "${INSTALL_SCRIPT_DIR}/09-smoke.sh"
echo "ok smoke verify"
