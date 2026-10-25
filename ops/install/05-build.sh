#!/usr/bin/env bash
set -eu
# shellcheck source=../common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
require_root

BUILD_DIR="${INSTALL_ROOT}/cpp/build"
cmake -S "${INSTALL_ROOT}/cpp" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
  -DSWISSQUOTE_BUILD_TESTS=OFF
cmake --build "${BUILD_DIR}" -j"$(nproc)"
cmake --install "${BUILD_DIR}"

chown root:"${SERVICE_USER}" "${INSTALL_PREFIX}/bin" "${FEED_BIN}" "${WRITER_BIN}"
chmod 750 "${INSTALL_PREFIX}/bin" "${FEED_BIN}" "${WRITER_BIN}"
python3 "${INSTALL_ROOT}/ops/install/migrate_ipc_config.py" "${CONFIG_PATH}" "${SOCKET_PATH}"

log "binaries installed"
