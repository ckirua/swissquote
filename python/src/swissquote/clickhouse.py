"""ClickHouse helpers for Swissquote BBO data."""

from __future__ import annotations

import os
from dataclasses import dataclass
from typing import Any, Final

DEFAULT_DATABASE: Final[str] = "swissquote"
DEFAULT_TABLE: Final[str] = "bbo"

BBO_COLUMNS: Final[tuple[str, ...]] = (
    "ts",
    "symbol",
    "platform",
    "server",
    "spread_profile",
    "bid_price",
    "ask_price",
    "bid_spread",
    "ask_spread",
)


@dataclass(frozen=True, slots=True)
class ClickHouseConfig:
    host: str
    port: int = 9000
    user: str = "default"
    password: str = ""

    @classmethod
    def from_env(cls) -> ClickHouseConfig:
        _load_dotenv()
        host = os.getenv("CLICKHOUSE_HOST")
        if not host:
            raise RuntimeError("CLICKHOUSE_HOST is not set")
        return cls(
            host=host,
            port=int(os.getenv("CLICKHOUSE_PORT", "9000")),
            user=os.getenv("CLICKHOUSE_USER", "default"),
            password=os.getenv("CLICKHOUSE_PASSWORD", ""),
        )


def _load_dotenv() -> None:
    try:
        from dotenv import load_dotenv
    except ImportError:
        return
    load_dotenv(os.path.expanduser("~/.env"))


def create_client(config: ClickHouseConfig | None = None):
    """Return a clickhouse-driver sync client."""
    try:
        from clickhouse_driver import Client
    except ImportError as exc:
        raise RuntimeError("install swissquote[databases] for ClickHouse support") from exc

    cfg = config or ClickHouseConfig.from_env()
    return Client(
        host=cfg.host,
        port=cfg.port,
        user=cfg.user,
        password=cfg.password,
    )


def query_bbo(
    client,
    *,
    database: str = DEFAULT_DATABASE,
    table: str = DEFAULT_TABLE,
    symbol: str | None = None,
    limit: int = 100,
) -> list[tuple[Any, ...]]:
    """Fetch recent BBO rows (newest first)."""
    params: dict[str, Any] = {"limit": int(limit)}
    where = ""
    if symbol is not None:
        where = "WHERE symbol = %(symbol)s"
        params["symbol"] = symbol
    cols = ", ".join(BBO_COLUMNS)
    sql = f"""
        SELECT {cols}
        FROM {database}.{table}
        {where}
        ORDER BY ts DESC
        LIMIT %(limit)s
    """
    return client.execute(sql, params)


def fetch_recent_bbo(
    *,
    symbol: str | None = None,
    limit: int = 100,
    config: ClickHouseConfig | None = None,
) -> list[tuple[Any, ...]]:
    """Connect from env and return recent BBO rows."""
    client = create_client(config)
    return query_bbo(client, symbol=symbol, limit=limit)


__all__: list[str] = [
    "BBO_COLUMNS",
    "ClickHouseConfig",
    "DEFAULT_DATABASE",
    "DEFAULT_TABLE",
    "create_client",
    "fetch_recent_bbo",
    "query_bbo",
]
