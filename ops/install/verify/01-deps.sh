#!/usr/bin/env bash
set -eu
# shellcheck source=../common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../common.sh"

bash "${REPO_ROOT}/cpp/scripts/check_toolchain.sh"
echo "ok deps"
