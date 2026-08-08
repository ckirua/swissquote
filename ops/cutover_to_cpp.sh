#!/usr/bin/env bash
# Stop Python services and cut over to C++ binaries only.
# Run: sudo bash ops/cutover_to_cpp.sh
set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec bash "${REPO_ROOT}/ops/deploy.sh"
