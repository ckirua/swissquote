"""Tests for post-decode BBO row validation."""

from __future__ import annotations

from datetime import UTC, datetime

from swissquote.codec.validate import is_valid_bbo_row

_GOOD_TS = datetime(2026, 7, 10, 12, 0, 0, tzinfo=UTC)


def _row(
    *,
    ts: datetime = _GOOD_TS,
    symbol: str = "EUR/USD",
    platform: str = "SwissquoteCapitalMarkets",
    server: str = "Live7",
    profile: str = "premium",
    bid: float = 1.1,
    ask: float = 1.11,
) -> tuple[object, ...]:
    return (ts, symbol, platform, server, profile, bid, ask, 0.5, 0.5)


def test_valid_row_accepts_fixture_like_quote() -> None:
    assert is_valid_bbo_row(_row())


def test_rejects_apital_markets_skew() -> None:
    assert not is_valid_bbo_row(_row(platform="apitalMarkets"))


def test_rejects_1900_timestamp() -> None:
    assert not is_valid_bbo_row(_row(ts=datetime(1900, 1, 1, 8, 50, 12, tzinfo=UTC)))


def test_rejects_xau_fx_scale_price() -> None:
    assert not is_valid_bbo_row(_row(symbol="XAU/USD", bid=0.56, ask=0.57))


def test_rejects_crossed_book() -> None:
    assert not is_valid_bbo_row(_row(bid=1.2, ask=1.1))
