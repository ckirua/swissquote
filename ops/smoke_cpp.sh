#!/usr/bin/env bash
# Deterministic end-to-end smoke: local HTTP fixture -> C++ feed -> Unix SOCK_SEQPACKET -> C++ writer -> HTTP fixture.
#
# Prerequisites: a built swissquote-feed and swissquote-writer (the script checks
# cpp/build first, then ${INSTALL_PREFIX}/bin), and Python 3 from the standard
# library.  No Swissquote credentials, network access, ClickHouse installation,
# or Python messaging package is required. The feed's test-only
# SWISSQUOTE_QUOTES_BASE_URL seam is pointed at the ephemeral local fixture.
set -Eeuo pipefail

PROFILE_DURATION=""
if [[ "${1:-}" == "--profile" ]]; then
  if [[ "$#" -ne 2 || ! "${2}" =~ ^[0-9]+$ ]] || (( ${#2} > 5 )) || (( 10#${2} < 1 || 10#${2} > 86400 )); then
    echo "Usage: $0 [--profile DURATION_SECONDS (1-86400)]" >&2
    exit 2
  fi
  PROFILE_DURATION=$((10#${2}))
elif [[ "$#" -ne 0 ]]; then
  echo "Usage: $0 [--profile DURATION_SECONDS (1-86400)]" >&2
  exit 2
fi
PROFILE_MODE=0
[[ -z "${PROFILE_DURATION}" ]] || PROFILE_MODE=1
ARTIFACT_PREFIX="swissquote-smoke"
if (( PROFILE_MODE )); then
  ARTIFACT_PREFIX="swissquote-cpu-profile"
fi

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INSTALL_PREFIX="${INSTALL_PREFIX:-${HOME}/.local}"
FEED_INSTALL="${INSTALL_PREFIX}/bin/swissquote-feed"
WRITER_INSTALL="${INSTALL_PREFIX}/bin/swissquote-writer"
FEED_BUILD="${REPO_ROOT}/cpp/build/swissquote-feed"
WRITER_BUILD="${REPO_ROOT}/cpp/build/swissquote-writer"

if [[ -x "${FEED_BUILD}" ]]; then
  FEED_BIN="${FEED_BUILD}"
elif [[ -x "${FEED_INSTALL}" ]]; then
  FEED_BIN="${FEED_INSTALL}"
else
  echo "ERROR: swissquote-feed is not built (expected ${FEED_BUILD} or ${FEED_INSTALL})" >&2
  exit 1
fi

if [[ -x "${WRITER_BUILD}" ]]; then
  WRITER_BIN="${WRITER_BUILD}"
elif [[ -x "${WRITER_INSTALL}" ]]; then
  WRITER_BIN="${WRITER_INSTALL}"
else
  echo "ERROR: swissquote-writer is not built (expected ${WRITER_BUILD} or ${WRITER_INSTALL})" >&2
  exit 1
fi

command -v python3 >/dev/null 2>&1 || {
  echo "ERROR: python3 is required for the local HTTP fixture" >&2
  exit 1
}

TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/${ARTIFACT_PREFIX}.XXXXXX")"
if (( PROFILE_MODE )); then
  echo "Profile artifacts: ${TMP_DIR}"
fi
QUOTE_PORT_FILE="${TMP_DIR}/quote.port"
CLICKHOUSE_PORT_FILE="${TMP_DIR}/clickhouse.port"
CLICKHOUSE_DATA="${TMP_DIR}/clickhouse.tsv"
CONFIG="${TMP_DIR}/service.toml"
FIXTURE_LOG="${TMP_DIR}/fixture.log"
FEED_LOG="${TMP_DIR}/feed.log"
WRITER_LOG="${TMP_DIR}/writer.log"
FEED_PID=""
WRITER_PID=""
FIXTURE_PID=""

cleanup() {
  set +e
  [[ -z "${FEED_PID}" ]] || kill "${FEED_PID}" 2>/dev/null || true
  [[ -z "${WRITER_PID}" ]] || kill "${WRITER_PID}" 2>/dev/null || true
  [[ -z "${FIXTURE_PID}" ]] || kill "${FIXTURE_PID}" 2>/dev/null || true
  [[ -z "${FEED_PID}" ]] || wait "${FEED_PID}" 2>/dev/null || true
  [[ -z "${WRITER_PID}" ]] || wait "${WRITER_PID}" 2>/dev/null || true
  [[ -z "${FIXTURE_PID}" ]] || wait "${FIXTURE_PID}" 2>/dev/null || true
  if (( ! PROFILE_MODE )); then
    rm -rf "${TMP_DIR}"
  fi
}
trap cleanup EXIT
if (( PROFILE_MODE )); then
  trap 'exit 130' INT
  trap 'exit 143' TERM
fi

if (( PROFILE_MODE )); then
  mkdir -p "${TMP_DIR}/logs"
fi
LOG_LEVEL="DEBUG"
BATCH_SIZE=1
RECV_TIMEOUT_MS=100
STARTUP_DELAY_SEC=0.0
ASYNC_INSERT=false
if (( PROFILE_MODE )); then
  LOG_LEVEL="INFO"
  BATCH_SIZE=5000
  RECV_TIMEOUT_MS=1000
  STARTUP_DELAY_SEC=0.5
  ASYNC_INSERT=true
fi

cat >"${CONFIG}" <<EOF
[ipc]
socket_path = "${TMP_DIR}/feed.sock"

[logging]
level = "${LOG_LEVEL}"
log_dir = "${TMP_DIR}/logs"

[writer]
batch_size = ${BATCH_SIZE}
batch_interval_sec = 0.1
recv_timeout_ms = ${RECV_TIMEOUT_MS}
startup_delay_sec = ${STARTUP_DELAY_SEC}
ch_database = "swissquote"
ch_table = "bbo"
async_insert = ${ASYNC_INSERT}

[feed]
weekend_pause = false
EOF

# The fixture serves every production symbol and records the writer's exact TSV
# POST.  Ports are selected by the OS, so concurrent smoke runs do not collide.
python3 - "${QUOTE_PORT_FILE}" "${CLICKHOUSE_PORT_FILE}" "${CLICKHOUSE_DATA}" >"${FIXTURE_LOG}" 2>&1 <<'PY' &
import json
import sys
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from threading import Lock, Thread
from urllib.parse import unquote
quote_port_file = Path(sys.argv[1])
clickhouse_port_file = Path(sys.argv[2])
clickhouse_data = Path(sys.argv[3])
quote_ts = int(time.time() * 1000)
clickhouse_lock = Lock()

class QuoteHandler(BaseHTTPRequestHandler):
    def log_message(self, *_args):
        pass

    def do_GET(self):
        prefix = "/public-quotes/bboquotes/instrument/"
        if not self.path.startswith(prefix):
            self.send_error(404)
            return
        symbol = unquote(self.path[len(prefix):]).split("?", 1)[0]
        if not symbol or "/" not in symbol:
            self.send_error(404)
            return
        # Keep all rows valid, including metals, while XAU/AUD remains the
        # deterministic assertion target below.
        bid = 2000.0 if symbol.startswith(("XAU/", "XAG/", "XPD/", "XPT/")) else 1.13768
        body = json.dumps([{
            "topo": {"platform": "SwissquoteCapitalMarkets", "server": "Live7"},
            "spreadProfilePrices": [{
                "spreadProfile": "prime",
                "bidSpread": 0.4,
                "askSpread": 0.4,
                "bid": bid,
                "ask": bid + 0.00030,
            }],
            "ts": quote_ts,
        },], separators=(",", ":")).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

class ClickHouseHandler(BaseHTTPRequestHandler):
    def log_message(self, *_args):
        pass

    def do_POST(self):
        size = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(size)
        with clickhouse_lock:
            with clickhouse_data.open("ab") as output:
                output.write(body)
                output.flush()
        response = b"Ok.\n"
        self.send_response(200)
        self.send_header("Content-Length", str(len(response)))
        self.end_headers()
        self.wfile.write(response)

quote_server = ThreadingHTTPServer(("127.0.0.1", 0), QuoteHandler)
clickhouse_server = ThreadingHTTPServer(("127.0.0.1", 0), ClickHouseHandler)
quote_port_file.write_text(str(quote_server.server_address[1]))
clickhouse_port_file.write_text(str(clickhouse_server.server_address[1]))
quote_thread = Thread(target=quote_server.serve_forever, daemon=True)
quote_thread.start()
try:
    clickhouse_server.serve_forever()
finally:
    quote_server.shutdown()
    quote_server.server_close()
    clickhouse_server.server_close()
PY
FIXTURE_PID=$!

wait_for_file() {
  local path="$1"
  local label="$2"
  for _ in $(seq 1 200); do
    [[ -s "${path}" ]] && return 0
    if ! kill -0 "${FIXTURE_PID}" 2>/dev/null; then
      echo "ERROR: local ${label} fixture exited" >&2
      cat "${FIXTURE_LOG}" >&2 || true
      exit 1
    fi
    sleep 0.05
  done
  echo "ERROR: local ${label} fixture did not become ready" >&2
  cat "${FIXTURE_LOG}" >&2 || true
  exit 1
}

wait_for_file "${QUOTE_PORT_FILE}" "quote"
wait_for_file "${CLICKHOUSE_PORT_FILE}" "ClickHouse"
QUOTE_PORT="$(<"${QUOTE_PORT_FILE}")"
CLICKHOUSE_PORT="$(<"${CLICKHOUSE_PORT_FILE}")"

# The socket path is local to this smoke run, isolating concurrent invocations.

CLICKHOUSE_HOST=127.0.0.1 \
CLICKHOUSE_HTTP_PORT="${CLICKHOUSE_PORT}" \
DOTENV_PATH="${TMP_DIR}/missing.env" \
"${WRITER_BIN}" --config "${CONFIG}" >"${WRITER_LOG}" 2>&1 &
WRITER_PID=$!
if (( PROFILE_MODE )); then
  sleep 1
else
  sleep 0.2
fi
if ! kill -0 "${WRITER_PID}" 2>/dev/null; then
  echo "ERROR: C++ writer exited before receiving a frame" >&2
  cat "${WRITER_LOG}" >&2 || true
  exit 1
fi

SWISSQUOTE_QUOTES_BASE_URL="http://127.0.0.1:${QUOTE_PORT}" \
CLICKHOUSE_HOST=127.0.0.1 \
CLICKHOUSE_HTTP_PORT="${CLICKHOUSE_PORT}" \
DOTENV_PATH="${TMP_DIR}/missing.env" \
"${FEED_BIN}" --config "${CONFIG}" >"${FEED_LOG}" 2>&1 &
FEED_PID=$!

for _ in $(seq 1 400); do
  if [[ -s "${CLICKHOUSE_DATA}" ]] && grep -q $'\tXAU/AUD\t' "${CLICKHOUSE_DATA}"; then
    break
  fi
  if ! kill -0 "${FEED_PID}" 2>/dev/null; then
    echo "ERROR: C++ feed exited before publishing the deterministic XAU/AUD frame" >&2
    cat "${FEED_LOG}" >&2 || true
    exit 1
  fi
  if ! kill -0 "${WRITER_PID}" 2>/dev/null; then
    echo "ERROR: C++ writer exited before inserting the deterministic XAU/AUD frame" >&2
    cat "${WRITER_LOG}" >&2 || true
    exit 1
  fi
  sleep 0.05
done
if [[ ! -s "${CLICKHOUSE_DATA}" ]] || ! grep -q $'\tXAU/AUD\t' "${CLICKHOUSE_DATA}"; then
  echo "ERROR: timed out waiting for C++ feed -> SOCK_SEQPACKET -> writer -> XAU/AUD ClickHouse assertion" >&2
  echo "--- feed ---" >&2
  cat "${FEED_LOG}" >&2 || true
  echo "--- writer ---" >&2
  cat "${WRITER_LOG}" >&2 || true
  echo "--- fixture payload ---" >&2
  cat "${CLICKHOUSE_DATA}" >&2 || true
  exit 1
fi
if ! python3 - "${CLICKHOUSE_DATA}" <<'PY'
import math
import sys
from pathlib import Path

rows = []
for line in Path(sys.argv[1]).read_text().splitlines():
    fields = line.split("\t")
    if len(fields) == 9:
        rows.append(fields)

matches = [row for row in rows if row[1] == "XAU/AUD"]
if not matches:
    raise SystemExit(f"no XAU/AUD row in ClickHouse fixture payload (rows={len(rows)})")
row = matches[0]
if row[2:5] != ["SwissquoteCapitalMarkets", "Live7", "prime"]:
    raise SystemExit(f"unexpected decoded metadata: {row[2:5]}")
if not math.isclose(float(row[5]), 2000.0, rel_tol=0.0, abs_tol=1e-9):
    raise SystemExit(f"unexpected XAU/AUD bid: {row[5]}")
print(f"ok  C++ feed -> SOCK_SEQPACKET -> writer -> ClickHouse fixture rows={len(rows)} symbol=XAU/AUD")
PY
then
  echo "ERROR: decoded C++ writer payload failed deterministic assertion" >&2
  echo "--- fixture payload ---" >&2
  cat "${CLICKHOUSE_DATA}" >&2 || true
  exit 1
fi

if (( PROFILE_MODE )); then
  row_count_before="$(python3 - "${CLICKHOUSE_DATA}" <<'PY'
import sys
from pathlib import Path
path = Path(sys.argv[1])
print(sum(1 for line in path.read_text().splitlines() if len(line.split("\t")) == 9))
PY
)"
  feed_drop_totals() {
    python3 - "${FEED_LOG}" <<'PY'
import re
import sys
from pathlib import Path
matches = re.findall(
    r"Unix IPC drops: queue=(\d+) no_client=(\d+) client=(\d+) send_errors=(\d+)",
    Path(sys.argv[1]).read_text(),
)
print(" ".join(matches[-1]) if matches else "0 0 0 0")
PY
  }
  feed_drops_before="$(feed_drop_totals)"
  echo "Running sustained local fixture profile for ${PROFILE_DURATION}s (starting_rows=${row_count_before}, feed_drop_totals=${feed_drops_before})."
  for ((elapsed = 0; elapsed < PROFILE_DURATION; elapsed++)); do
    if ! kill -0 "${FEED_PID}" 2>/dev/null; then
      echo "ERROR: C++ feed exited during profile run" >&2
      exit 1
    fi
    if ! kill -0 "${WRITER_PID}" 2>/dev/null; then
      echo "ERROR: C++ writer exited during profile run" >&2
      exit 1
    fi
    if ! kill -0 "${FIXTURE_PID}" 2>/dev/null; then
      echo "ERROR: local fixtures exited during profile run" >&2
      exit 1
    fi
    sleep 1
  done
  feed_drops_after="$(feed_drop_totals)"
  if [[ "${feed_drops_after}" != "${feed_drops_before}" ]]; then
    echo "ERROR: feed drop/error counters changed during profile (before=${feed_drops_before}, after=${feed_drops_after})" >&2
    exit 1
  fi

  row_count="$(python3 - "${CLICKHOUSE_DATA}" <<'PY'
import sys
from pathlib import Path
path = Path(sys.argv[1])
print(sum(1 for line in path.read_text().splitlines() if len(line.split("\t")) == 9))
PY
)"
  if (( row_count <= row_count_before )); then
    echo "ERROR: sustained profile did not grow ClickHouse fixture rows (before=${row_count_before}, after=${row_count})" >&2
    exit 1
  fi
  if grep -Eiq 'recv_errors=[1-9][0-9]*|decode_errors=[1-9][0-9]*|failures=[1-9][0-9]*|flush failed|\[(error|critical)\]' "${WRITER_LOG}"; then
    echo "ERROR: writer log reports receive, decode, or flush errors" >&2
    exit 1
  fi
  echo "Profile passed: rows=${row_count} duration=${PROFILE_DURATION}s artifacts=${TMP_DIR}"
else
  echo "Smoke passed."
fi

# Stop cleanly; EXIT cleanup also covers failures and retains profile artifacts.
kill "${FEED_PID}" 2>/dev/null || true
kill "${WRITER_PID}" 2>/dev/null || true
wait "${FEED_PID}" 2>/dev/null || true
wait "${WRITER_PID}" 2>/dev/null || true
FEED_PID=""
WRITER_PID=""
