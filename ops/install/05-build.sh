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

chown root:"${SERVICE_USER}" "${FEED_BIN}" "${WRITER_BIN}"
chmod 750 "${FEED_BIN}" "${WRITER_BIN}"

log "binaries installed"
