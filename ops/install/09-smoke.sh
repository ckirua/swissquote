#!/usr/bin/env bash
set -eu
# shellcheck source=../common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

[[ -x "${FEED_BIN}" ]] || exit 1
systemctl is-active --quiet "${FEED_UNIT}" || exit 1
systemctl is-active --quiet "${WRITER_UNIT}" || exit 1

if [[ "${SKIP_CLICKHOUSE}" != "1" && -f "${ENV_PATH}" ]]; then
  # shellcheck disable=SC1090
  set -a
  source "${ENV_PATH}"
  set +a
  if command -v clickhouse-client >/dev/null; then
    before="$(clickhouse-client --host "${CLICKHOUSE_HOST}" --port "${CLICKHOUSE_PORT:-9000}" \
      --user "${CLICKHOUSE_USER:-default}" --password "${CLICKHOUSE_PASSWORD:-}" \
      --query "SELECT count() FROM swissquote.bbo" 2>/dev/null || echo 0)"
    sleep 5
    after="$(clickhouse-client --host "${CLICKHOUSE_HOST}" --port "${CLICKHOUSE_PORT:-9000}" \
      --user "${CLICKHOUSE_USER:-default}" --password "${CLICKHOUSE_PASSWORD:-}" \
      --query "SELECT count() FROM swissquote.bbo" 2>/dev/null || echo 0)"
    if [[ "${after}" -lt "${before}" ]]; then
      echo "WARN row count decreased (${before} -> ${after})"
    fi
    log "ClickHouse rows: ${before} -> ${after}"
  fi
fi

log "smoke passed"
echo "ok smoke"
