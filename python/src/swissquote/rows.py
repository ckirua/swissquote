"""Named row view over Cython decode tuples."""

from __future__ import annotations

from datetime import datetime
from typing import NamedTuple

from swissquote.codec import flatten_bbo_frame as _flatten_bbo_frame
from swissquote.codec.validate import filter_valid_bbo_rows


class BboRow(NamedTuple):
    ts: datetime
    symbol: str
    platform: str
    server: str
    spread_profile: str
    bid_price: float
    ask_price: float
    bid_spread: float
    ask_spread: float


def flatten_bbo_rows(frame: bytes) -> list[BboRow]:
    """Decode an SBE frame into named rows (wraps Cython flatten_bbo_frame)."""
    raw_rows = _flatten_bbo_frame(frame)
    valid_rows, _rejected = filter_valid_bbo_rows(raw_rows)
    return [BboRow(*row) for row in valid_rows]


__all__: list[str] = ["BboRow", "flatten_bbo_rows"]
