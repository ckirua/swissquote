# Swissquote C++26 Engine

Native production engine for the Swissquote BBO pipeline:

```
HTTP quotes → JSON→SBE encode → Unix-domain SOCK_SEQPACKET → decode → ClickHouse
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

Build and run the optional Unix-socket/shared-memory IPC benchmark:

```bash
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release \
  -DSWISSQUOTE_BUILD_BENCHMARKS=ON
cmake --build cpp/build --target swissquote_ipc_benchmark -j
cpp/build/benchmarks/swissquote_ipc_benchmark \
  --repetitions 5 --burst-size 87 --frame-count 100000
```

The benchmark waits for the receiver to validate each frame before publishing
the next one. This prevents the nonblocking Unix publisher from disconnecting
the benchmark consumer under an artificial producer burst; the receiver's
acknowledgement is outside the reported one-way latency.

`EncodeWorkspace` and reusable decoder vectors retain peak capacity on purpose;
they are thread-confined and sequentially reusable. The Unix IPC ring limits
frames to 65,536 bytes. The feed broadcasts each SBE frame as one
`AF_UNIX/SOCK_SEQPACKET` message to connected local consumers; slow clients are
disconnected rather than blocking feed workers.

The writer copies borrowed frame strings into a batch-owned byte arena.
Rows keep offsets rather than pointers, so arena growth does not invalidate
stored rows. `RowBuffer::view()` resolves borrowed strings for serialization;
those views must not survive a buffer mutation. `clear()` retains row and arena
capacity for reuse, and failed inserts retain the owned batch for retries.

Release writer benchmark allocation counts (normal: 9 rows; large: 216 rows):

| Materialization | Previous normal | Arena normal | Previous large | Arena large |
| --- | ---: | ---: | ---: | ---: |
| Fresh batch | 8 | 11 | 81 | 20 |
| Retained batch | 3 | 0 | 72 | 0 |

Fresh small batches allocate more with the arena; the production writer retains
its batch storage. An isolated weekend real-quote experiment used one feed and
two writers with disposable ClickHouse tables. Across five matched runs, each
writer inserted 9,396 rows per run with identical distinct quote values and
zero rejected rows or decode errors. Arena cycles/row were lower in 4/5 runs:
the median fell from 13,323.73 to 12,946.75 (2.83%), but the reduction of
376.98 cycles/row was below twice the baseline MAD (891.26 cycles/row).
This fails the noise gate; the arena remains experimental, not a demonstrated
production CPU improvement. Three separate 60-second stack captures per process
were sparse, and the baseline writers consumed only 213 ms of CPU across all
five counter runs. No further application hotspot was selected.

The ClickHouse client retains its TSV payload buffer between successful or
failed insert attempts; wire fields, precision, and UTC timestamps are unchanged.

Run the deterministic reusable-path allocation regression (it exits nonzero
if either reusable operation allocates after warmup):

```bash
cpp/build/benchmarks/swissquote_codec_allocation_regression
```

## Binaries

| Binary | Role |
|--------|------|
| `swissquote-feed` | Poll Swissquote REST API, encode SBE, broadcast over Unix IPC |
| `swissquote-writer` | Unix-domain SOCK_SEQPACKET consumer, decode, batch insert ClickHouse |

Both accept `--config ~/.config/service.toml` and print `--version`.

### Weekend pause

When `[feed].weekend_pause = true` (default), `swissquote-feed` skips REST polls from
**Fri 16:55 → Sun 16:50 America/New_York** (DST-safe via IANA `tzdata`). The process
stays up; `swissquote-writer` is not paused. Set `weekend_pause = false` to disable.

## Toolchain

Run `cpp/scripts/check_toolchain.sh` before building. Requires GCC 13+ or Clang 17+
with C++23 support (`-std=c++23`; C++26 when available).
