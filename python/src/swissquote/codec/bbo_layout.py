"""BBO SBE wire layout constants (pure Python; mirror bbo_layout.pxi)."""

from __future__ import annotations

from typing import Final

SBE_HEADER_SIZE: Final = 8
BBO_SCHEMA_ID: Final = 1
BBO_TEMPLATE_ID: Final = 1
BBO_SCHEMA_VERSION: Final = 1
ROOT_BLOCK_LENGTH: Final = 12
VENUE_BLOCK_LENGTH: Final = 16
PROFILE_BLOCK_LENGTH: Final = 28

__all__: list[str, ...] = [
    "BBO_SCHEMA_ID",
    "BBO_SCHEMA_VERSION",
    "BBO_TEMPLATE_ID",
    "PROFILE_BLOCK_LENGTH",
    "ROOT_BLOCK_LENGTH",
    "SBE_HEADER_SIZE",
    "VENUE_BLOCK_LENGTH",
]
