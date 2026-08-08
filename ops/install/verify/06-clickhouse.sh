#!/usr/bin/env bash
set -eu
# shellcheck source=../common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../common.sh"

if [[ "${SKIP_CLICKHOUSE}" == "1" ]]; then
  if ! curl -fsS --max-time 10 "http://${HOST}:${CLICKHOUSE_HTTP_PORT:-8123}/ping" >/dev/null; then
    exit 1
  fi
  echo "ok clickhouse (skipped schema)"
  exit 0
fi

# shellcheck disable=SC1090
set -a
source "${ENV_PATH}"
set +a

HOST="${CLICKHOUSE_HOST:-127.0.0.1}"
NATIVE_PORT="${CLICKHOUSE_PORT:-9000}"
USER="${CLICKHOUSE_USER:-default}"
PASS="${CLICKHOUSE_PASSWORD:-}"

command -v clickhouse-client >/dev/null || exit 1
count="$(clickhouse-client --host "${HOST}" --port "${NATIVE_PORT}" --user "${USER}" --password "${PASS}" \
  --query "SELECT count() FROM system.tables WHERE database='swissquote' AND name='bbo'" 2>/dev/null || echo 0)"
[[ "${count}" == "1" ]] || exit 1
echo "ok clickhouse"
