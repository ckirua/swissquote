#!/usr/bin/env bash
# Print the ClickHouse password used by the Swissquote stack.
#
# The same value is read by:
#   - swissquote-writer              (EnvironmentFile /etc/swissquote/env)
#   - Grafana ClickHouse datasource  (EnvironmentFile /root/.env)
#   - swissquote-bbo-archive         (EnvironmentFile /root/.env)
#
# It is stored only in 0600 root-owned files, so run with sudo. The password
# is printed to stdout — capture it into a variable; do not echo it to a log.
#
# Usage:
#   sudo bash ops/get_ch_password.sh                  # read /etc/swissquote/env
#   sudo bash ops/get_ch_password.sh /root/.env       # or a specific file
#   clickhouse-client --password "$(sudo bash ops/get_ch_password.sh)" \
#       --query 'SELECT count() FROM swissquote.bbo FINAL'
set -euo pipefail

ENV_FILE="${1:-/etc/swissquote/env}"

if [[ ! -r "${ENV_FILE}" ]]; then
  echo "error: cannot read ${ENV_FILE} (0600 root — run with sudo)" >&2
  exit 1
fi

pw="$(grep -E '^CLICKHOUSE_PASSWORD=' "${ENV_FILE}" 2>/dev/null | head -n1 | cut -d= -f2- || true)"

if [[ -z "${pw}" ]]; then
  echo "error: CLICKHOUSE_PASSWORD not found in ${ENV_FILE}" >&2
  exit 1
fi

printf '%s\n' "${pw}"
