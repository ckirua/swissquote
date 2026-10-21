#!/usr/bin/env bash
set -eu
# shellcheck source=../common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../common.sh"

[[ -x "${FEED_BIN}" ]] || exit 1
[[ -x "${WRITER_BIN}" ]] || exit 1
"${FEED_BIN}" --help >/dev/null 2>&1 || "${FEED_BIN}" --version >/dev/null 2>&1
grep -Fqx '[ipc]' "${CONFIG_PATH}"
grep -Fqx "socket_path = \"${SOCKET_PATH}\"" "${CONFIG_PATH}"
echo "ok build"
