#!/usr/bin/env bash
set -eu
# shellcheck source=../common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
require_root

# shellcheck disable=SC1090
set -a
source "${ENV_PATH}"
set +a

HOST="${CLICKHOUSE_HOST:-127.0.0.1}"
HTTP_PORT="${CLICKHOUSE_HTTP_PORT:-8123}"
NATIVE_PORT="${CLICKHOUSE_PORT:-9000}"
USER="${CLICKHOUSE_USER:-default}"
PASS="${CLICKHOUSE_PASSWORD:-}"

clickhouse_ping() {
  if ! curl -fsS --max-time 10 "http://${HOST}:${HTTP_PORT}/ping" >/dev/null; then
    die "ClickHouse HTTP ping failed (${HOST}:${HTTP_PORT})"
  fi
  if ! command -v clickhouse-client >/dev/null; then
    log "clickhouse-client not installed; HTTP ping ok"
    return 0
  fi
  if ! clickhouse-client \
    --host "${HOST}" \
    --port "${NATIVE_PORT}" \
    --user "${USER}" \
    --password "${PASS}" \
    --query "SELECT 1" >/dev/null 2>&1; then
    die "ClickHouse native auth/query failed (${HOST}:${NATIVE_PORT})"
  fi
  log "ClickHouse reachable at ${HOST}"
}

if [[ "${WITH_LOCAL_CLICKHOUSE}" == "1" ]]; then
  export DEBIAN_FRONTEND=noninteractive
  if ! command -v clickhouse-client >/dev/null; then
    apt-get update -qq
    apt-get install -y --no-install-recommends clickhouse-server clickhouse-client
  fi
  systemctl enable clickhouse-server
  systemctl start clickhouse-server
  for _ in $(seq 1 30); do
    if clickhouse-client --query "SELECT 1" >/dev/null 2>&1; then
      break
    fi
    sleep 2
  done
  HOST="127.0.0.1"
fi

clickhouse_ping

if [[ "${SKIP_CLICKHOUSE}" == "1" ]]; then
  log "skip-clickhouse: schema apply skipped"
  exit 0
fi

if ! command -v clickhouse-client >/dev/null; then
  die "clickhouse-client not found (install locally or on remote host)"
fi

clickhouse-client \
  --host "${HOST}" \
  --port "${NATIVE_PORT}" \
  --user "${USER}" \
  --password "${PASS}" \
  --multiquery <"${INSTALL_ROOT}/sql/swissquote-bbo.db.sql"

log "schema applied on ${HOST}"
