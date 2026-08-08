"""Parity tests for Cython flatten_bbo_frame."""

from __future__ import annotations

import pytest

from swissquote.codec import flatten_bbo_frame, flatten_bbo_frame_into

from .fixtures import EMPTY_FRAME, EXPECTED_FLATTEN_ROWS, SAMPLE_FRAME, SAMPLE_SYMBOL


def _assert_rows_match(actual: list[tuple], expected: list[tuple]) -> None:
    assert len(actual) == len(expected)
    for act, exp in zip(actual, expected, strict=True):
        assert act[0] == exp[0]
        assert act[1:] == pytest.approx(exp[1:], rel=0, abs=1e-6)


def test_flatten_bbo_frame_matches_golden_rows() -> None:
    _assert_rows_match(flatten_bbo_frame(SAMPLE_FRAME), EXPECTED_FLATTEN_ROWS)


def test_flatten_bbo_frame_nine_rows() -> None:
    rows = flatten_bbo_frame(SAMPLE_FRAME)
    assert len(rows) == 9
    assert rows[0][1] == SAMPLE_SYMBOL
    assert rows[0][4] == "premium"


def test_flatten_empty_snapshot() -> None:
    assert flatten_bbo_frame(EMPTY_FRAME) == []


def test_flatten_bbo_frame_into_matches_flatten() -> None:
    direct = flatten_bbo_frame(SAMPLE_FRAME)
    buf: list[tuple] = []
    n = flatten_bbo_frame_into(SAMPLE_FRAME, buf)
    assert n == len(direct)
    _assert_rows_match(buf, direct)


def test_flatten_bbo_frame_into_accumulates() -> None:
    buf: list[tuple] = []
    n1 = flatten_bbo_frame_into(SAMPLE_FRAME, buf)
    n2 = flatten_bbo_frame_into(SAMPLE_FRAME, buf)
    assert n1 == 9
    assert n2 == 9
    assert len(buf) == 18


def test_flatten_bbo_frame_into_invalid_raises() -> None:
    buf: list[tuple] = []
    with pytest.raises(ValueError):
        flatten_bbo_frame_into(b"\x00" * 4, buf)
    assert buf == []
