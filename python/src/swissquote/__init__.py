"""Swissquote Python client — subscribe, fetch quotes, query ClickHouse."""

from swissquote.__about__ import __version__
from swissquote.codec import flatten_bbo_frame, flatten_bbo_frame_into
from swissquote.constants import SWISSQUOTE_SYMBOLS as SYMBOLS, validate_symbol
from swissquote.http import REST_URL, fetch_quotes
from swissquote.rows import BboRow, flatten_bbo_rows
from swissquote.subscriber import BboSubscriber

__all__: list[str] = [
    "BboRow",
    "BboSubscriber",
    "REST_URL",
    "SYMBOLS",
    "__version__",
    "fetch_quotes",
    "flatten_bbo_frame",
    "flatten_bbo_frame_into",
    "flatten_bbo_rows",
    "validate_symbol",
]
