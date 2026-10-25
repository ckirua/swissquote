# Swissquote

Swissquote Data Low-Latency Engine — HTTP quotes → SBE encode → Unix socket feed → decode → ClickHouse.

Production services run on the **C++ engine** under [`cpp/`](cpp/). Use the **Python client** in [`python/`](python/) to subscribe, fetch quotes, and query ClickHouse (Cython decode, no server build).

```bash
cd cpp
bash scripts/install_deps.sh --check  # or omit --check to install via apt
make build
make test
```

Binaries install to `~/.local/bin/` via `cmake --install` (see deploy below).

## Python client

```bash
cd python
pip install -e ".[databases]"
pytest tests/ -q
```

The feed subscriber uses a Unix-domain `SOCK_SEQPACKET` socket. The default
path is `/run/swissquote/feed.sock`; override it with `--socket-path`:

```bash
python scripts/examples/receiver.py --socket-path /run/swissquote/feed.sock
```

See [`python/README.md`](python/README.md) for subscribe, REST, and ClickHouse examples.

## Dev deploy (home tree)

Use this on a developer machine with a git checkout under `~/projects/swissquote`.

Prerequisites: `cmake`, `g++` (13+), `libssl-dev`, `python3`, `~/.config/service.toml`, `~/.env` with `CLICKHOUSE_HOST`.

```bash
bash ops/deploy-check.sh              # preflight (no root)
sudo bash ops/deploy.sh               # build → ~/.local/bin, dev systemd units
bash ops/smoke_cpp.sh                 # Unix IPC + decode smoke
```

For a sustained isolated fixture run, use `ops/smoke_cpp.sh --profile SECONDS`.
It verifies continued row growth and feed/writer health while retaining run
artifacts under `/tmp/swissquote-cpu-profile.*`.


| Unit | Binary |
|------|--------|
| `swissquote-feed.service` | `~/.local/bin/swissquote-feed` |
| `swissquote-writer.service` | `~/.local/bin/swissquote-writer` |

Dev unit files live in [`systemd/dev/`](systemd/dev/) (home paths).


## Production install (`/opt`, gated epochs)

Prod installs to `/opt/swissquote` with config in `/etc/swissquote`. Each epoch runs and verifies before the next; resume after a failure with `--from-epoch N`.

```bash
# Dry-run preflight (read-only)
sudo bash install.sh --dry-run

# Staging on same host (does not touch live units)
sudo bash install.sh --staging --skip-clickhouse \
  --env-file /home/swissquote/.env

# New server from git archive
git archive HEAD | ssh newhost 'dir=$(mktemp -d) && tar x -C "$dir" && cd "$dir" && \
  sudo CLICKHOUSE_HOST=YOUR_CLICKHOUSE_HOST bash install.sh'

# Resume after epoch 5 failure
sudo bash /opt/swissquote/install.sh --from-epoch 5

# Rollback staging/prod units
sudo bash /opt/swissquote/ops/install/rollback.sh --staging
sudo bash /opt/swissquote/ops/install/rollback.sh --restore
```

| Epoch | Action |
|-------|--------|
| 0 | Preflight (OS, HTTPS, ports) |
| 1 | apt build deps |
| 2 | user + `/opt`, `/etc`, `/var/lib` dirs |
| 3 | `/etc/swissquote/service.toml` + `env` |
| 4 | rsync release tree to `/opt/swissquote` |
| 5 | cmake build + install binaries |
| 6 | ClickHouse (optional local or external schema) |
| 7 | render systemd units from `systemd/*.service.in` |
| 8 | start services |
| 9 | smoke + optional row-count check |

Prod preflight after install: `DEPLOY_MODE=prod bash ops/deploy-check.sh`

Staging preflight: `DEPLOY_MODE=prod INSTALL_ROOT=/opt/swissquote-staging CONFIG_DIR=/etc/swissquote-staging bash ops/deploy-check.sh`

Prod upgrades on an installed tree: `sudo bash /opt/swissquote/install.sh --from-epoch 4` (sync tree) or `--from-epoch 5` (rebuild only).

Logs: `journalctl -u swissquote-feed -f` / `journalctl -u swissquote-writer -f`

The production writer emits an INFO diagnostics summary once per minute:
`journalctl -u swissquote-writer -f`. It reports enqueued/consumed frame rates,
ClickHouse-accepted row rate, sampled IPC queue depth, queue push attempt failures
(including retries; not necessarily lost frames), frame size, decode/rejection
counts, flush results, and decode/materialization/serialization/HTTP timings.
With async inserts enabled, successful HTTP acceptance does not mean ClickHouse
has finished inserting the rows. The subscriber and publisher queues hold up to 2048
frames each. Slot buffers grow lazily to frame size and are reused; startup
does not reserve the theoretical 128 MiB maximum per ring. Under a full backlog
of maximum-size frames, each ring can still grow to that bound.

The C++ writer uses ClickHouse **HTTP on port 8123** (set `CLICKHOUSE_HTTP_PORT` in env file).

## Environment variables

| Variable | Required | Default | Purpose |
|----------|----------|---------|---------|
| `CLICKHOUSE_HOST` | yes (writer / client queries) | — | ClickHouse host |
| `CLICKHOUSE_HTTP_PORT` | no | `8123` | HTTP insert port (C++ writer) |
| `CLICKHOUSE_PORT` | no | `9000` | Native TCP port (Python client queries) |
| `CLICKHOUSE_USER` | no | `default` | Username |
| `CLICKHOUSE_PASSWORD` | no | `""` | Password |

## Grafana (BBO monitoring)

Local Grafana + ClickHouse datasource provisioning lives in [`grafana/`](grafana/).
Apply with `sudo bash grafana/apply-provisioning.sh`, then open
`http://127.0.0.1:3000/` (SSH tunnel if remote). See [`grafana/README.md`](grafana/README.md).

## ClickHouse schema

Apply [`sql/swissquote-bbo.db.sql`](sql/swissquote-bbo.db.sql) before running the writer
(epoch 6 does this on install). That file creates the database plus:

| Object | Role |
|--------|------|
| `swissquote.bbo` | Raw ticks (`ReplacingMergeTree`); ALIAS cols `mid` / `spread_*` / `quote_age_ms` |
| `swissquote.bbo_mv` | Plain VIEW over `bbo` (same columns including ALIAS-derived mid/spread/quote_age) |

Existing hosts that already have `bbo`: apply the idempotent upgrade
[`sql/swissquote-bbo-features.sql`](sql/swissquote-bbo-features.sql) (`ALTER` ALIAS cols + `CREATE OR REPLACE VIEW bbo_mv`).

`bbo` uses `ReplacingMergeTree(ingestion_time)` — `count(*)` may fluctuate until
background merges dedupe rows with the same quote key; use `count(*) FINAL` for stable totals.
Quote `ts` reflects market timestamps (stale on weekends); `ingestion_time` tracks live inserts.

## S3 archive

The production installer enables `swissquote-bbo-archive.timer`, which exports
the previous UTC calendar day's `swissquote.bbo` rows to
`s3://${S3_SWISSQUOTE_BUCKET}/archive/bbo/year=YYYY/week=WW/` as Parquet every
day at 06:00 UTC. Empty days are skipped; an existing object is left untouched,
so reruns are idempotent. The service reads ClickHouse and S3 credentials from
`/root/.env`. Run `sudo bash install.sh --from-epoch 4` to deploy the archive
script and timer alongside the C++ release.

## Examples (manual)

```bash
~/.local/bin/swissquote-feed
~/.local/bin/swissquote-writer
python python/scripts/examples/receiver.py --socket-path /run/swissquote/feed.sock
python python/scripts/examples/fetch_bbo.py --symbol EUR/USD
```
