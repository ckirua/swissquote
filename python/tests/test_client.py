"""Client SDK tests."""

from __future__ import annotations

import pytest

from swissquote import flatten_bbo_rows, validate_symbol
from swissquote.rows import BboRow

from .codec.fixtures import SAMPLE_FRAME, SAMPLE_SYMBOL


def test_validate_symbol() -> None:
    assert validate_symbol("EUR/USD") == "EUR/USD"
    with pytest.raises(ValueError):
        validate_symbol("bad")


def test_flatten_bbo_rows_named() -> None:
    rows = flatten_bbo_rows(SAMPLE_FRAME)
    assert len(rows) == 9
    assert isinstance(rows[0], BboRow)
    assert rows[0].symbol == SAMPLE_SYMBOL
    assert rows[0].spread_profile == "premium"
