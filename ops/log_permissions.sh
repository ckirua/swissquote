#!/usr/bin/env bash
# Delegate to secure_permissions (keeps log dir setup in one place).
set -eu
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec bash "${SCRIPT_DIR}/secure_permissions.sh"
