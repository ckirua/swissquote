# Grafana — Swissquote BBO

Local Grafana OSS for monitoring `swissquote.bbo` / `swissquote.bbo_mv` via the
official **grafana-clickhouse-datasource** plugin against **local ClickHouse**
(`CLICKHOUSE_*` in `~/.env`). No ClickHouse Cloud / Cursor MCP.

## Access

| | |
|--|--|
| URL | `http://127.0.0.1:3000/` |
| Default user | `admin` |
| Bind | `127.0.0.1` only (see below) |

Grafana is bound to **localhost**. From your laptop:

```bash
ssh -L 3000:127.0.0.1:3000 user@this-vps
# then open http://127.0.0.1:3000/
```

### Admin password

- First-time install: login `admin` / `admin`, then Grafana forces a change.
- If the password is unknown, reset (as root):

```bash
cd /usr/share/grafana
grafana cli admin reset-admin-password 'YOUR_NEW_PASSWORD'
```

## Apply / refresh provisioning

```bash
sudo bash grafana/apply-provisioning.sh
# optional: ENV_FILE=/path/to/.env sudo -E bash grafana/apply-provisioning.sh
```

This will:

1. Install `grafana-clickhouse-datasource` if missing
2. Copy datasource + dashboard provider YAML into `/etc/grafana/provisioning/`
3. Copy dashboard JSON into `/var/lib/grafana/dashboards/swissquote/`
4. Add a systemd drop-in so grafana-server loads `CLICKHOUSE_*` from `~/.env`
5. Set `http_addr = 127.0.0.1` and restart Grafana

Repo files under `grafana/` are the source of truth; system paths are copies
because the `grafana` user cannot traverse `/root`.

## Provisioning layout

```
grafana/
  apply-provisioning.sh
  README.md
  dashboards/
    swissquote-bbo.json          # variables: symbol, spread_profile, server, clean, clean_lag, axis, ewma, y_min, y_max
  provisioning/
    datasources/
      clickhouse.yaml            # ${CLICKHOUSE_HOST|USER|PASSWORD}; native :9000
    dashboards/
      dashboards.yaml            # → /var/lib/grafana/dashboards/swissquote
```

### Datasource secrets

`provisioning/datasources/clickhouse.yaml` uses env expansion — **no passwords
in git**. The apply script wires:

`/etc/systemd/system/grafana-server.service.d/swissquote-clickhouse.conf`
→ `EnvironmentFile=-/root/.env`

Required vars (same as the rest of this repo):

| Variable | Used for |
|----------|----------|
| `CLICKHOUSE_HOST` | Plugin host (typically `127.0.0.1`) |
| `CLICKHOUSE_USER` | Username (often `default`) |
| `CLICKHOUSE_PASSWORD` | Password (may be empty) |
| `CLICKHOUSE_PORT` | Documented as native `9000`; YAML hardcodes `9000` + `protocol: native` |

To change the password later: edit `~/.env`, then
`sudo systemctl restart grafana-server`.

## Dashboard panels

All panels query `swissquote.bbo_mv` (VIEW over `bbo`), aggregate in SQL,
time-filtered with `$__timeFilter_ms(...)`:

| Row | Panel | Metric |
|-----|-------|--------|
| Price | Bid / Ask / Mid | 1m last: `argMax(bid_price\|ask_price\|mid, ts)`; gated by **Clean prices** |
| Spread & ops | Spread abs + EWMA | 1m last `argMax(spread_abs, ts)` (ask−bid) plus EWMA over **EWMA span**; same clean/weekend filters as price |
| Spread & ops | Quote rate | `count()` per minute |
| Spread & ops | Spread bps | 1m `quantile(0.5)(spread_bps)` |
| Spread & ops | Ingestion lag | 1m `avg(dateDiff('ms', ts, ingestion_time))` on `ingestion_time`; gated by **Clean lag** |

Variables (defaults chosen for live liquid data):

| Variable | Default | Notes |
|----------|---------|-------|
| **symbol** | `EUR/USD` | Query limited to symbols with quotes in the last 1 day |
| **spread_profile** | `premium` | Same 1-day filter |
| **server** | `Live7` | Prefer one venue so price lines stay clean; `All` still available |
| **axis** (Weekends) | `session` | Price + spread abs: `session` = hide NY weekend rows (real timestamps, zoom works; gap disconnects); `calendar` = show weekends. |
| **clean** (Clean prices) | `yes` | Bid/Ask/Mid and Spread abs. `yes` = ±5% day-median + `abs(spread_bps)<100`; `no` = only bid/ask > 0 and ask ≥ bid (junk spikes visible) |
| **clean_lag** (Clean lag) | `yes` | Ingestion lag only. `yes` = drop samples with lag ≥ 60s so the axis stays usable; `no` = raw spikes |
| **ewma** (EWMA span min) | `120` | Window for the Spread abs EWMA overlay (`20` / `60` / `120` / `240` / `480`) |
| **y_min** / **y_max** | blank | Leave blank for auto Y scale. Set both (e.g. `1.135` / `1.145` for EUR/USD) to **lock** the price axis while you zoom/pan time |

### Time picker

Dashboard quick ranges (top-right clock): Last 15m / 1h / 6h / 24h / 2d / 7d / 30d, plus **Full range** = actual `min(ts)` → `now`. Default open range = **min(full history, last 6 months)** → `now` (both patched from ClickHouse on every `apply-provisioning.sh` run).

### Price chart navigation

Grafana’s time series is **not** TradingView — **mouse wheel does not zoom/pan** the price chart.

| Action | How |
|--------|-----|
| Zoom in | Click-drag across the plot area |
| Zoom out | Double-click the plot |
| Pan time | Click-drag on the **X-axis** (timestamp bar under the series) |
| Keyboard | `t+` zoom in, `t-` zoom out (with dashboard focused) |
| Lock Y axis | Set **Y min** + **Y max** in the variable bar; clear both for auto |
| Break weekend / gap lines | Price panel uses **Disconnect values > 5m** (`insertNulls: 300000`) so Fri→Mon is not a straight bridge |

For true wheel-scroll / TV-style charting you’d need a custom web UI (e.g. Lightweight Charts), not a Grafana panel tweak.

Pick a single server for the price chart. `All` merges venues into one last-quote-per-minute series and is better for ops than for reading the path.

**Clean prices**, **Clean lag**, and **EWMA span** sit in the top variable bar next to Symbol / Spread profile / Server. Flip Clean prices to `no` for raw junk ticks; flip Clean lag to `no` to see absurd lag spikes.

## Verify

```bash
# Service + bind
systemctl is-active grafana-server
ss -tlnp | grep 3000   # expect 127.0.0.1:3000

# Plugin present
ls /var/lib/grafana/plugins/grafana-clickhouse-datasource

# After login: Connections → Data sources → ClickHouse → Save & test
# Dashboards → Swissquote → Swissquote BBO
```

## Firewall note

Prefer **localhost + SSH tunnel**. Do not expose `:3000` on `0.0.0.0` without
auth hardening / TLS / firewall allowlists. ClickHouse itself already listens on
`127.0.0.1:8123` / `127.0.0.1:9000` on this host.
