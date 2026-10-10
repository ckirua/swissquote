# Swissquote C++26 Engine

Native production engine for the Swissquote BBO pipeline:

```
HTTP quotes → JSON→SBE encode → ZMQ PUB → ZMQ SUB → decode → ClickHouse
```

## Deploy

**Dev** (home layout): `sudo bash ops/deploy.sh` — builds to `~/.local/bin`, installs [`systemd/dev/`](../systemd/dev/) units.

**Prod** (`/opt/swissquote`): gated epochs via [`install.sh`](../install.sh) at repo root. See the root [README](../README.md#production-install-opt-gated-epochs).

## Build

On Debian/Ubuntu, install the build dependencies manually:

```bash
bash scripts/install_deps.sh
```

Check an existing installation without changing the system:

```bash
bash scripts/install_deps.sh --check
```

Then build and test:

```bash
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build -j
ctest --test-dir cpp/build --output-on-failure
```

## Low-latency codec path

The feed keeps one `EncodeWorkspace` per worker thread, reusing simdjson
parser/input storage and SBE output capacity across polls. The writer can reuse
view-row capacity with
`flatten_bbo_frame_views(frame, reusable_rows)`. The return-by-value APIs remain
available for convenience.

Build and run the standalone codec benchmark:

```bash
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release \
  -DSWISSQUOTE_BUILD_CODEC_BENCHMARK=ON
cmake --build cpp/build --target swissquote_codec_benchmark swissquote_benchmarks -j
cpp/build/benchmarks/swissquote_codec_benchmark --iterations 100000 --warmup 5000
cpp/build/benchmarks/swissquote_benchmarks "[benchmark]" --reporter compact
```

The allocation benchmark reports both return-by-value and reusable paths.

`EncodeWorkspace` and reusable decoder vectors retain peak capacity on purpose;
they are thread-confined and sequentially reusable. The production SPSC ring
limits frames to 65,536 bytes. Do not shrink workspaces in the hot loop. If
future inputs are allowed above that bound, add an explicit opt-in trim/reset
operation and measure its memory tradeoff first.

The remaining writer cost is intentional ownership conversion from borrowed
frame views into batch-owned strings, followed by ClickHouse TSV serialization.
Those steps span frames and retries, so they cannot be removed without changing
batch ownership semantics.

Run the deterministic reusable-path allocation regression (it exits nonzero
if either reusable operation allocates after warmup):

```bash
cpp/build/benchmarks/swissquote_codec_allocation_regression
```

## Binaries

| Binary | Role |
|--------|------|
| `swissquote-feed` | Poll Swissquote REST API, encode SBE, ZMQ PUB |
| `swissquote-writer` | ZMQ SUB, decode, batch insert ClickHouse |

Both accept `--config ~/.config/service.toml` and print `--version`.

### Weekend pause

When `[feed].weekend_pause = true` (default), `swissquote-feed` skips REST polls from
**Fri 16:55 → Sun 16:50 America/New_York** (DST-safe via IANA `tzdata`). The process
stays up; `swissquote-writer` is not paused. Set `weekend_pause = false` to disable.

## Toolchain

Run `cpp/scripts/check_toolchain.sh` before building. Requires GCC 13+ or Clang 17+
with C++23 support (`-std=c++23`; C++26 when available).
