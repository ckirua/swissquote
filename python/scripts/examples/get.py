"""Fetch one instrument quote over HTTP."""

from __future__ import annotations

import sys

from swissquote import fetch_quotes


def main() -> None:
    symbol = sys.argv[1] if len(sys.argv) > 1 else "EUR/USD"
    print(fetch_quotes(symbol).decode())


if __name__ == "__main__":
    main()
