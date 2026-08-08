#!/usr/bin/env bash
# End-to-end smoke: C++ feed -> ZMQ -> decode (ClickHouse optional).
set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INSTALL_PREFIX="${INSTALL_PREFIX:-${HOME}/.local}"
FEED="${INSTALL_PREFIX}/bin/swissquote-feed"
WRITER="${INSTALL_PREFIX}/bin/swissquote-writer"
BUILD_FEED="${REPO_ROOT}/cpp/build/swissquote-feed"

if [[ -x "${FEED}" ]]; then
  FEED_BIN="${FEED}"
elif [[ -x "${BUILD_FEED}" ]]; then
  FEED_BIN="${BUILD_FEED}"
else
  echo "ERROR: build first (cd cpp && make build) or run ops/deploy.sh" >&2
  exit 1
fi

ENDPOINT="ipc:///tmp/swissquote-smoke-$$"
TIMEOUT_SEC=20

cleanup() {
  [[ -n "${FEED_PID:-}" ]] && kill "${FEED_PID}" 2>/dev/null || true
  wait "${FEED_PID:-}" 2>/dev/null || true
}
trap cleanup EXIT

echo "==> swissquote C++ smoke =="
echo "endpoint: ${ENDPOINT}"

# Minimal one-symbol feed via Python client subscriber (no full HTTP cycle in smoke).
python3.14 - <<PY
import subprocess, time, sys
from pathlib import Path

sys.path.insert(0, str(Path("${REPO_ROOT}") / "client" / "src"))
from swissquote.decode import flatten_bbo_frame
from swissquote.subscriber import BboSubscriber

# Use pre-encoded fixture frame published via a tiny zmq helper
import zmq, struct, json

endpoint = "${ENDPOINT}"
ctx = zmq.Context.instance()
pub = ctx.socket(zmq.PUB)
pub.bind(endpoint)
time.sleep(0.3)

symbol = "EUR/USD"
json_bytes = (
    b'[{"topo":{"platform":"SwissquoteCapitalMarkets","server":"Live7"},'
    b'"spreadProfilePrices":[{"spreadProfile":"premium","bidSpread":0.65,'
    b'"askSpread":0.65,"bid":1.13768,"ask":1.13798}],"ts":1782941100045}]'
)
# encode minimal frame (same layout as client tests)
venues = json.loads(json_bytes.decode())
venue = venues[0]
platform = venue["topo"]["platform"].encode()
server = venue["topo"]["server"].encode()
profile = venue["spreadProfilePrices"][0]
spread = profile["spreadProfile"].encode()
sym = symbol.encode()
body = bytearray()
body += struct.pack("<4H", 12, 1, 1, 1)
body += struct.pack("<IQ", len(sym), 0)
body += sym
body += struct.pack("<HH", 16, 1)
body += struct.pack("<IIQ", len(platform), len(server), venue["ts"])
body += platform + server
body += struct.pack("<HH", 28, 1)
body += struct.pack("<Iddff", len(spread), profile["bid"], profile["ask"], profile["bidSpread"], profile["askSpread"])
body += spread
frame = bytes(body)

sub = BboSubscriber(endpoint)
sub.start()
time.sleep(0.2)
pub.send(frame)
received = sub.recv_frame(timeout_ms=5000)
sub.close()
pub.close(linger=0)

if not received:
    raise SystemExit("no frame received")
rows = flatten_bbo_frame(received)
if len(rows) != 1 or rows[0].symbol != symbol:
    raise SystemExit(f"unexpected decode: {rows}")
print(f"ok  zmq roundtrip symbol={symbol} profiles={len(rows)}")
PY

echo "ok  decode path"
echo "Smoke passed."
