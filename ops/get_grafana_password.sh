#!/usr/bin/env bash
# Print the Grafana admin password used by the Swissquote BBO dashboard.
#
# It is written to the provisioning env file (default /root/.env) as
# GRAFANA_ADMIN_PASSWORD by grafana/apply-provisioning.sh and lives there as a
# 0600 root-owned file, so run with sudo. The value is printed to stdout —
# capture it into a variable; do not echo it to a log.
#
# Usage:
#   sudo bash ops/get_grafana_password.sh                 # read /root/.env
#   sudo bash ops/get_grafana_password.sh /root/.env      # or a specific file
#   # log in to http://127.0.0.1:3000/ as admin with the printed password
set -euo pipefail

ENV_FILE="${1:-/root/.env}"

if [[ ! -r "${ENV_FILE}" ]]; then
  echo "error: cannot read ${ENV_FILE} (0600 root — run with sudo)" >&2
  exit 1
fi

pw="$(grep -E '^GRAFANA_ADMIN_PASSWORD=' "${ENV_FILE}" 2>/dev/null | head -n1 | cut -d= -f2- || true)"

if [[ -z "${pw}" ]]; then
  echo "error: GRAFANA_ADMIN_PASSWORD not found in ${ENV_FILE}" >&2
  echo "       (run grafana/apply-provisioning.sh first)" >&2
  exit 1
fi

printf '%s\n' "${pw}"
