# Changelog

All notable changes to the Swissquote C++26 engine and Python client follow
[Keep a Changelog](https://keepachangelog.com/) and [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- Daily ClickHouse-to-S3 Parquet archive for Swissquote BBO, scheduled at 06:00 UTC.

### Changed
- Production C++ codec now uses the generated SBE headers from `sbe/generated/` for frame encoding and decoding; wire layout and compatibility tests remain unchanged.
- Writer now reserves configured batch capacity and reuses ClickHouse TSV payload storage across flushes; retained normal-batch materialization measured 3 allocations versus 8 for a fresh batch.
- Removed the ZeroMQ transport and dependency; feed, writer, and Python subscribers now exchange complete SBE frames over a local Unix-domain SOCK_SEQPACKET socket.

### Fixed
- Install script creates `LOG_DIR` owned by the service user so the feed can write logs on fresh install.

## [0.2.2] - 2026-08-25

### Added
- Grafana **Swissquote BBO** dashboard: spread abs + EWMA overlay, **Clean lag** outlier filter, weekend/session axis (synced from `arathi-research/grafana` `dashboards/swissquote/`)

- `swissquote-feed` weekend pause: skip REST polls Fri 16:55–Sun 16:50 America/New_York
  when `[feed].weekend_pause` is enabled (default true); writer stays up

## [0.2.1] - 2026-07-05

### Changed
- `python/` is now a **client-only** package: Cython decode, ZMQ subscribe, HTTP quotes, ClickHouse helpers
- Removed legacy Python/Cython server (emitter, writer, ZMQ pub/sub build targets)
- Removed duplicate `client/` tree; consolidated into `python/`

### Added
- `swissquote.clickhouse.fetch_recent_bbo()` for one-liner queries from `~/.env`
- `scripts/examples/fetch_bbo.py` CLI

## [0.2.0] - 2026-07-05

### Added
- C++26 production engine (`cpp/`): `libswissquote`, `swissquote-feed`, `swissquote-writer`
- SBE encode/decode with `std::expected`, Catch2 parity tests
- Threaded ZMQ PUB/SUB with SPSC ring
- TOML/env config, spdlog logging, httplib HTTP client
- ClickHouse HTTP TabSeparated batch insert
- Python client SDK (`python/`): Cython decode, subscribe, HTTP quotes, ClickHouse
- `cpp/Makefile`, `cpp/scripts/check_toolchain.sh`

### Changed
- systemd units point to C++ binaries
- `ops/deploy.sh` builds and deploys C++ engine
- Legacy `python/` server removed (v0.2.1 client-only)
- `sql/swissquote-bbo.db.sql` aligned with production `ReplacingMergeTree(ingestion_time)`

### Fixed
- HTTPS quote fetch: use `httplib::SSLClient` (plain `Client` on port 443 failed all requests)
- ClickHouse async insert: pass `async_insert` as URL params (multi-statement HTTP rejected)
- `ops/_permissions.sh`: fix `.env` permission apply (same-path `install` crash)

### Gates passed
- `ctest` — codec + ZMQ roundtrip (5/5)
- `pytest python/tests/` — client SDK (8/8)

## [0.1.0-dev.0] - 2026-07-05

### Added
- C++26 refactor bootstrap on `feature/cpp26-refactor`
- `cpp/VERSION`, `CHANGELOG.md`, CMake stub, Python client skeleton
- Iteration delivery loop documented in `cpp/README.md`

### Gates passed
- `cmake -S cpp -B cpp/build` configures successfully
