#!/usr/bin/env bash
# Production install with gated epochs (/opt/swissquote + /etc/swissquote).
set -eu

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

STAGING=0
DRY_RUN=0
FORCE=0
SKIP_CLICKHOUSE=0
WITH_LOCAL_CLICKHOUSE=0
FROM_EPOCH=0
ONLY_EPOCH=""
ENV_FILE_INPUT=""

usage() {
  cat <<EOF
Usage: sudo bash install.sh [options]

Options:
  --staging                 Use /opt/swissquote-staging and *-staging.service units
  --from-epoch N            Run epochs N..9 (default: 0)
  --epoch N                 Run only epoch N
  --dry-run                 Print plan; epoch 0 checks only
  --force                   Skip conflicting-service guard in preflight
  --skip-clickhouse         Skip ClickHouse install/schema (epoch 6 verify only)
  --with-local-clickhouse   apt install clickhouse-server locally (epoch 6)
  --env-file PATH           Copy secrets from existing env file
  -h, --help                Show help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --staging) STAGING=1; shift ;;
    --dry-run) DRY_RUN=1; shift ;;
    --force) FORCE=1; shift ;;
    --skip-clickhouse) SKIP_CLICKHOUSE=1; shift ;;
    --with-local-clickhouse) WITH_LOCAL_CLICKHOUSE=1; shift ;;
    --from-epoch) FROM_EPOCH="$2"; shift 2 ;;
    --epoch) ONLY_EPOCH="$2"; shift 2 ;;
    --env-file) ENV_FILE_INPUT="$2"; shift 2 ;;
    -h | --help) usage; exit 0 ;;
    *) echo "ERROR: unknown option: $1" >&2; usage; exit 2 ;;
  esac
done

export STAGING DRY_RUN FORCE SKIP_CLICKHOUSE WITH_LOCAL_CLICKHOUSE FROM_EPOCH ONLY_EPOCH ENV_FILE_INPUT

# shellcheck source=ops/install/common.sh
source "${SCRIPT_DIR}/ops/install/common.sh"

print_plan
if [[ -f "${STATE_FILE}" ]]; then
  # shellcheck disable=SC1090
  source "${STATE_FILE}"
  echo "Last passed epoch (state file): ${LAST_PASSED_EPOCH:--1}"
fi

run_epoch 0 preflight "Preflight"
if [[ "${DRY_RUN}" == "1" ]]; then
  echo "Dry-run complete (epoch 0 only)."
  exit 0
fi

require_root

run_epoch 1 deps "OS packages"
run_epoch 2 user-dirs "Service user and directories"
run_epoch 3 config "Prod config"
run_epoch 4 install-tree "Release tree"
run_epoch 5 build "Build and install"
run_epoch 6 clickhouse "ClickHouse"
run_epoch 7 systemd "Systemd units"
run_epoch 8 start "Start services"
run_epoch 9 smoke "End-to-end smoke"

echo
echo "Install complete."
echo "  systemctl status ${FEED_UNIT} ${WRITER_UNIT}"
echo "  journalctl -u ${WRITER_UNIT} -n 30 --no-pager"
