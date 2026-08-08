"""Post-decode validation for flattened BBO rows.

Corrupt ZMQ/SBE frames can decode without raising when length fields still fit
inside the buffer. Typical signatures:

- ``platform='apitalMarkets'`` — 13-byte skew into ``SwissquoteCapitalMarkets``
- ``ts`` year 1900 — ``venueTsMs`` read from misaligned wire bytes
- metal symbols with FX-scale prices (e.g. XAU/USD bid ~0.56)

Reject bad rows at ingest; use ``scripts/cleanup_bbo.py`` to purge history.
"""

from __future__ import annotations

from datetime import UTC, datetime, timedelta
from typing import Any, Final

VALID_PLATFORMS: Final[frozenset[str]] = frozenset(
    {"AT", "SwissquoteCapitalMarkets", "SwissquoteLtd"}
)
VALID_SERVERS: Final[frozenset[str]] = frozenset({"AT", "Live5", "Live7"})

MIN_QUOTE_TS: Final[datetime] = datetime(2020, 1, 1, tzinfo=UTC)
MAX_QUOTE_TS_SKEW: Final[timedelta] = timedelta(days=1)

METAL_MIN_BID: Final[tuple[tuple[str, float], ...]] = (
    ("XAU/", 100.0),
    ("XPD/", 100.0),
    ("XPT/", 100.0),
    ("XAG/", 1.0),
)


def _coerce_ts(value: Any) -> datetime | None:
    if value is None:
        return None
    if isinstance(value, datetime):
        return value if value.tzinfo is not None else value.replace(tzinfo=UTC)
    return None


def _metal_min_bid(symbol: str) -> float | None:
    for prefix, floor in METAL_MIN_BID:
        if symbol.startswith(prefix):
            return floor
    return None


def is_valid_bbo_row(row: tuple[Any, ...] | Any, *, now: datetime | None = None) -> bool:
    """Return True when a flattened row looks like a real quote."""
    if isinstance(row, tuple):
        if len(row) < 8:
            return False
        ts, symbol, platform, server, _profile, bid, ask, *_rest = row
    else:
        ts = getattr(row, "ts", None) or getattr(row, "ts_ms", None)
        symbol = row.symbol
        platform = row.platform
        server = row.server
        bid = row.bid_price
        ask = row.ask_price

    ts_dt = _coerce_ts(ts)
    if ts_dt is None:
        if isinstance(ts, int):
            ts_dt = datetime.fromtimestamp(ts / 1000, tz=UTC)
        else:
            return False

    ref = now or datetime.now(tz=UTC)
    if ts_dt < MIN_QUOTE_TS or ts_dt > ref + MAX_QUOTE_TS_SKEW:
        return False
    if platform not in VALID_PLATFORMS or server not in VALID_SERVERS:
        return False
    if not isinstance(symbol, str) or "/" not in symbol:
        return False

    try:
        bid_f = float(bid)
        ask_f = float(ask)
    except (TypeError, ValueError):
        return False
    if bid_f <= 0.0 or ask_f <= 0.0 or bid_f > ask_f:
        return False

    metal_floor = _metal_min_bid(symbol)
    if metal_floor is not None and bid_f < metal_floor:
        return False
    return True


def filter_valid_bbo_rows(rows: list[tuple[Any, ...]]) -> tuple[list[tuple[Any, ...]], int]:
    """Drop invalid rows; return (kept, rejected_count)."""
    kept: list[tuple[Any, ...]] = []
    rejected = 0
    for row in rows:
        if is_valid_bbo_row(row):
            kept.append(row)
        else:
            rejected += 1
    return kept, rejected
