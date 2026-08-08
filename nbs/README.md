# Notebooks

Exploratory analysis of Swissquote BBO in ClickHouse (`bbo` ticks; optional VIEW `bbo_mv`).

## Setup

Activate the project venv first:

```bash
source /home/swissquote/.venvs/swissquote/bin/activate
```

Then install deps:

```bash
cd ../python
pip install -e ".[databases]"

cd ../nbs
pip install -r requirements.txt
```

Ensure `~/.env` defines `CLICKHOUSE_HOST` (and optional `CLICKHOUSE_PORT`, `CLICKHOUSE_USER`, `CLICKHOUSE_PASSWORD`).

Native TCP port **9000** must be reachable from your machine (the C++ writer uses HTTP on 8123; notebooks use `clickhouse-driver` on 9000).

Schema: fresh installs use [`../sql/swissquote-bbo.db.sql`](../sql/swissquote-bbo.db.sql). Existing DBs need
[`../sql/swissquote-bbo-features.sql`](../sql/swissquote-bbo-features.sql) (ALIAS columns + plain VIEW `bbo_mv`).

## Helpers (`ch_utils.py`)

- `load_bbo_df` — tick rows; selects ALIAS columns `mid` / `spread_*` / `quote_age_ms`
- `resample_bbo_chart` — resample tick series for day/week/history plots

## Notebooks

| Notebook | Description |
|----------|-------------|
| `01_bbo_overview.ipynb` | Connect, table stats, sample load, symbol/platform counts |
| `02_spreads_eda.ipynb` | Mid/spread distributions and resampled day/week/history TS from ticks |

Run Jupyter Lab from this directory (with venv active):

```bash
source /home/swissquote/.venvs/swissquote/bin/activate
jupyter lab
```

## Benchmark full-symbol load

```bash
source /home/swissquote/.venvs/swissquote/bin/activate
cd nbs
python bench_load.py --symbol "XAU/USD"
```
