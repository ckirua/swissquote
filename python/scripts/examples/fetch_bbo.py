"""Print recent BBO rows from ClickHouse."""

from __future__ import annotations

import argparse

from swissquote.clickhouse import fetch_recent_bbo


def main() -> None:
    parser = argparse.ArgumentParser(description="Fetch recent swissquote.bbo rows")
    parser.add_argument("--symbol", default=None, help="e.g. EUR/USD")
    parser.add_argument("--limit", type=int, default=10)
    args = parser.parse_args()

    rows = fetch_recent_bbo(symbol=args.symbol, limit=args.limit)
    for row in rows:
        print(row)


if __name__ == "__main__":
    main()
