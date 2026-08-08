#!/usr/bin/env python3
"""Benchmark full-symbol BBO load (e.g. XAU/USD)."""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

NBS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(NBS_DIR))

from ch_utils import load_bbo_df, symbol_availability  # noqa: E402
from swissquote.clickhouse import ClickHouseConfig, create_client  # noqa: E402


def main() -> None:
    parser = argparse.ArgumentParser(description="Benchmark ClickHouse BBO load")
    parser.add_argument("--symbol", default="XAU/USD")
    parser.add_argument("--target-seconds", type=float, default=8 * 60)
    args = parser.parse_args()

    client = create_client(ClickHouseConfig.from_env())
    avail = symbol_availability(client, args.symbol)
    print(f"symbol={args.symbol} rows={avail['row_count']:,}")
    print(f"ts={avail['ts_min']} -> {avail['ts_max']}")

    started = time.perf_counter()
    df = load_bbo_df(
        client,
        symbol=args.symbol,
        hours=None,
        limit=None,
        enrich=True,
        sort=True,
    )
    elapsed = time.perf_counter() - started
    print(f"loaded={len(df):,} elapsed={elapsed:.2f}s ({elapsed / 60:.2f}m)")
    print(f"target={'PASS' if elapsed < args.target_seconds else 'FAIL'} (<{args.target_seconds:.0f}s)")


if __name__ == "__main__":
    main()
