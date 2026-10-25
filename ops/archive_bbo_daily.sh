#!/usr/bin/env bash
# Export the previous UTC day's Swissquote BBO rows to Hetzner S3 as Parquet.
# S3 layout: archive/bbo/year=YYYY/week=WW/bbo_YYYYMMDD.parquet
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: archive_bbo_daily.sh [--date YYYY-MM-DD] [--dry-run]

Defaults to yesterday in UTC. Existing S3 objects are skipped; safe to rerun.
EOF
}

target_day="$(date -u -d yesterday +%F)"
dry_run=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --date) target_day="${2:?--date requires YYYY-MM-DD}"; shift 2 ;;
    --dry-run) dry_run=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done
if ! parsed_day="$(date -u -d "$target_day" +%F 2>/dev/null)" || [[ "$parsed_day" != "$target_day" ]]; then
  echo "ERROR: invalid UTC date: $target_day" >&2
  exit 2
fi

ENV_FILE="${ENV_FILE:-/root/.env}"
if [[ -f "$ENV_FILE" ]]; then
  set -a
  # shellcheck disable=SC1090
  source <(sed 's/\r$//' "$ENV_FILE" | sed 's/[[:space:]]*$//')
  set +a
fi

: "${CLICKHOUSE_HOST:=127.0.0.1}"
: "${CLICKHOUSE_PORT:=9000}"
: "${CLICKHOUSE_USER:=default}"
: "${CLICKHOUSE_PASSWORD:?CLICKHOUSE_PASSWORD required}"
: "${S3_SWISSQUOTE_BUCKET:?S3_SWISSQUOTE_BUCKET required}"
: "${S3_URL:?S3_URL required}"
: "${S3_ACCESS_KEY:?S3_ACCESS_KEY required}"
: "${S3_SECRET_KEY:?S3_SECRET_KEY required}"
: "${S3_REGION:=fsn1}"

export AWS_ACCESS_KEY_ID="${S3_ACCESS_KEY// /}"
export AWS_SECRET_ACCESS_KEY="${S3_SECRET_KEY// /}"
export AWS_DEFAULT_REGION="$S3_REGION"

log() { echo "[$(date -u +%Y-%m-%dT%H:%M:%SZ)] $*"; }
s3_key="archive/bbo/year=$(date -u -d "$target_day" +%G)/week=$(date -u -d "$target_day" +%V)/bbo_${target_day//-/}.parquet"
if aws s3api head-object --bucket "$S3_SWISSQUOTE_BUCKET" --key "$s3_key" --endpoint-url "$S3_URL" >/dev/null 2>&1; then
  log "SKIP exists s3://${S3_SWISSQUOTE_BUCKET}/${s3_key}"
  exit 0
fi

row_count="$(clickhouse-client \
  --host "$CLICKHOUSE_HOST" --port "$CLICKHOUSE_PORT" \
  --user "$CLICKHOUSE_USER" --password "$CLICKHOUSE_PASSWORD" \
  --query "SELECT count() FROM swissquote.bbo WHERE toDate(ts) = '${target_day}'")"
if [[ "$row_count" == "0" ]]; then
  log "SKIP empty day=${target_day} (0 rows)"
  exit 0
fi
if [[ "$dry_run" == "1" ]]; then
  log "DRY-RUN would export ${row_count} rows day=${target_day} -> s3://${S3_SWISSQUOTE_BUCKET}/${s3_key}"
  exit 0
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
local_file="${tmpdir}/bbo_${target_day//-/}.parquet"
log "Export ${row_count} rows day=${target_day} -> ${s3_key}"
clickhouse-client \
  --host "$CLICKHOUSE_HOST" --port "$CLICKHOUSE_PORT" \
  --user "$CLICKHOUSE_USER" --password "$CLICKHOUSE_PASSWORD" \
  --query "SELECT ts, symbol, platform, server, spread_profile, bid_price, ask_price, bid_spread, ask_spread, ingestion_time FROM swissquote.bbo WHERE toDate(ts) = '${target_day}' FORMAT Parquet" > "$local_file"
if [[ ! -s "$local_file" ]]; then
  log "ERROR empty Parquet file for day=${target_day}" >&2
  exit 1
fi
aws s3 cp "$local_file" "s3://${S3_SWISSQUOTE_BUCKET}/${s3_key}" --endpoint-url "$S3_URL" --only-show-errors
log "Uploaded s3://${S3_SWISSQUOTE_BUCKET}/${s3_key} (${row_count} rows)"
