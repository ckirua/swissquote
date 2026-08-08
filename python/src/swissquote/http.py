"""HTTP client for Swissquote REST BBO quotes."""

from __future__ import annotations

from typing import Final

import httpx

from swissquote.constants import SWISSQUOTE_REST_URL, validate_symbol

REST_URL: Final[str] = SWISSQUOTE_REST_URL


def fetch_quotes(symbol: str, *, timeout: float = 30.0) -> bytes:
    """Fetch raw JSON bytes for one instrument."""
    validate_symbol(symbol)
    url = f"{REST_URL}/public-quotes/bboquotes/instrument/{symbol}"
    response = httpx.get(url, timeout=timeout)
    response.raise_for_status()
    return response.content


__all__: list[str] = ["REST_URL", "fetch_quotes"]
