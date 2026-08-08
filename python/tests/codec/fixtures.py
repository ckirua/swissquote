"""Test fixtures for BBO SBE decode."""

from __future__ import annotations

from datetime import UTC, datetime
from typing import Any

SAMPLE_SYMBOL: str = "EUR/USD"
SAMPLE_FRAME_LEN: int = 448

# Golden frame (448 bytes) from the legacy encoder; decode-only client tests use this.
SAMPLE_FRAME: bytes = (
    b"\x0c\x00\x01\x00\x01\x00\x01\x00\x07\x00\x00\x00\x00h\xe5\xcf\x8b\x01\x00\x00EUR/USD"
    b"\x10\x00\x03\x00\x18\x00\x00\x00\x05\x00\x00\x00\r(\x92\x1f\x9f\x01\x00\x00SwissquoteCapitalMarketsLive7"
    b"\x1c\x00\x03\x00\x07\x00\x00\x002\x03\x95\xf1\xef3\xf2?\x85\x088\x84*5\xf2?ff&?ff&?premium"
    b"\x05\x00\x00\x00\x15\x8cJ\xea\x044\xf2?0\xbb'\x0f\x0b5\xf2?\xcd\xcc\xcc>\xcd\xcc\xcc>prime"
    b"\x05\x00\x00\x00\xdc\x9d\xb5\xdb.4\xf2?i\xa9\xbc\x1d\xe14\xf2?\x00\x00\x00\x00\x00\x00\x00\x00elite"
    b"\x02\x00\x00\x00\x02\x00\x00\x00\r(\x92\x1f\x9f\x01\x00\x00ATAT"
    b"\x1c\x00\x03\x00\x08\x00\x00\x00Nz\xdf\xf8\xda3\xf2?h\x91\xed|?5\xf2?\x9a\x99Y?\x9a\x99Y?standard"
    b"\x07\x00\x00\x00\xc0>:u\xe53\xf2?\x85\x088\x84*5\xf2?333?333?premium"
    b"\x05\x00\x00\x00\xa4\xc7\xefm\xfa3\xf2?\x13D\xdd\x07 5\xf2?\xcd\xcc\x0c?\xcd\xcc\x0c?prime"
    b"\r\x00\x00\x00\x05\x00\x00\x00\r(\x92\x1f\x9f\x01\x00\x00SwissquoteLtdLive5"
    b"\x1c\x00\x03\x00\x07\x00\x00\x002\x03\x95\xf1\xef3\xf2?\x85\x088\x84*5\xf2?ff&?ff&?premium"
    b"\x05\x00\x00\x00\x15\x8cJ\xea\x044\xf2?0\xbb'\x0f\x0b5\xf2?\xcd\xcc\xcc>\xcd\xcc\xcc>prime"
    b"\x05\x00\x00\x00\xdc\x9d\xb5\xdb.4\xf2?i\xa9\xbc\x1d\xe14\xf2?\x00\x00\x00\x00\x00\x00\x00\x00elite"
)

EMPTY_FRAME: bytes = (
    b"\x0c\x00\x01\x00\x01\x00\x01\x00\x07\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00EUR/USD\x10\x00\x00\x00"
)

_VENUE_TS = datetime(2026, 7, 1, 21, 25, 0, 45000, tzinfo=UTC)

EXPECTED_FLATTEN_ROWS: list[tuple[Any, ...]] = [
    (_VENUE_TS, SAMPLE_SYMBOL, "SwissquoteCapitalMarkets", "Live7", "premium", 1.13768, 1.13798, 0.65, 0.65),
    (_VENUE_TS, SAMPLE_SYMBOL, "SwissquoteCapitalMarkets", "Live7", "prime", 1.1377, 1.13795, 0.4, 0.4),
    (_VENUE_TS, SAMPLE_SYMBOL, "SwissquoteCapitalMarkets", "Live7", "elite", 1.13774, 1.13791, 0.0, 0.0),
    (_VENUE_TS, SAMPLE_SYMBOL, "AT", "AT", "standard", 1.13766, 1.138, 0.85, 0.85),
    (_VENUE_TS, SAMPLE_SYMBOL, "AT", "AT", "premium", 1.13767, 1.13798, 0.7, 0.7),
    (_VENUE_TS, SAMPLE_SYMBOL, "AT", "AT", "prime", 1.13769, 1.13797, 0.55, 0.55),
    (_VENUE_TS, SAMPLE_SYMBOL, "SwissquoteLtd", "Live5", "premium", 1.13768, 1.13798, 0.65, 0.65),
    (_VENUE_TS, SAMPLE_SYMBOL, "SwissquoteLtd", "Live5", "prime", 1.1377, 1.13795, 0.4, 0.4),
    (_VENUE_TS, SAMPLE_SYMBOL, "SwissquoteLtd", "Live5", "elite", 1.13774, 1.13791, 0.0, 0.0),
]
