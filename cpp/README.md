# Swissquote C++26 Engine

Native production engine for the Swissquote BBO pipeline:

```
HTTP quotes → JSON→SBE encode → ZMQ PUB → ZMQ SUB → decode → ClickHouse
```

## Deploy

**Dev** (home layout): `sudo bash ops/deploy.sh` — builds to `~/.local/bin`, installs [`systemd/dev/`](../systemd/dev/) units.

**Prod** (`/opt/swissquote`): gated epochs via [`install.sh`](../install.sh) at repo root. See the root [README](../README.md#production-install-opt-gated-epochs).

## Build

```bash
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build -j
ctest --test-dir cpp/build --output-on-failure
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
