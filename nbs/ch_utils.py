"""Shared ClickHouse helpers for notebooks."""

from __future__ import annotations

from decimal import Decimal
from typing import Any, Final

import numpy as np
import pandas as pd

from swissquote.clickhouse import (
    BBO_COLUMNS,
    DEFAULT_DATABASE,
    DEFAULT_TABLE,
)

BBO_SELECT_COLUMNS: tuple[str, ...] = (*BBO_COLUMNS, "ingestion_time")

NUMERIC_COLUMNS: tuple[str, ...] = (
    "bid_price",
    "ask_price",
    "bid_spread",
    "ask_spread",
)
DERIVED_NUMERIC_COLUMNS: tuple[str, ...] = ("mid", "spread_abs", "spread_bps", "quote_age_ms")
ALL_NUMERIC_COLUMNS: tuple[str, ...] = (*NUMERIC_COLUMNS, *DERIVED_NUMERIC_COLUMNS)
DATETIME_COLUMNS: Final[frozenset[str]] = frozenset({"ts", "ingestion_time"})

BBO_LOAD_COLUMNS: tuple[str, ...] = (*BBO_SELECT_COLUMNS, *DERIVED_NUMERIC_COLUMNS)

# Prefer table ALIAS columns (mid/spread_*/quote_age_ms) over inlined expressions.
ENRICHED_SELECT_SQL: Final[str] = """
    ts,
    symbol,
    platform,
    server,
    spread_profile,
    bid_price,
    ask_price,
    bid_spread,
    ask_spread,
    ingestion_time,
    mid,
    spread_abs,
    spread_bps,
    quote_age_ms
"""

QUERY_SETTINGS: Final[dict[str, Any]] = {
    "max_threads": 0,
    "max_block_size": 65536,
}


def _coerce_value(value: Any) -> Any:
    if isinstance(value, Decimal):
        return float(value)
    return value


def _as_float64(column: Any) -> np.ndarray:
    arr = np.asarray(column)
    if arr.dtype == object:
        return np.asarray(pd.to_numeric(arr, errors="coerce"), dtype=np.float64)
    return np.asarray(arr, dtype=np.float64)


def _as_datetime(column: Any) -> pd.Series:
    return pd.to_datetime(column, utc=True)


def _build_frame(names: list[str], columnar_data: list[Any]) -> pd.DataFrame:
    """Build a DataFrame from ClickHouse columnar blocks without extra copies."""
    payload: dict[str, Any] = {}
    for name, column in zip(names, columnar_data, strict=True):
        if name in ALL_NUMERIC_COLUMNS:
            payload[name] = _as_float64(column)
        elif name in DATETIME_COLUMNS:
            payload[name] = _as_datetime(column)
        else:
            payload[name] = column
    return pd.DataFrame(payload)


def _sort_by_ts(df: pd.DataFrame, column: str = "ts") -> pd.DataFrame:
    if df.empty or column not in df.columns:
        return df.reset_index(drop=True)
    ts_values = df[column].to_numpy(dtype="datetime64[ns]", copy=False)
    order = np.argsort(ts_values, kind="mergesort")
    return df.iloc[order].reset_index(drop=True)


def coerce_float_series(series: pd.Series) -> pd.Series:
    """Convert ClickHouse Decimal/object numerics to float64."""
    return pd.Series(_as_float64(series.to_numpy()), index=series.index, copy=False)


def _fetch_dataframe(
    client,
    sql: str,
    params: dict[str, Any],
    *,
    columns: tuple[str, ...],
    settings: dict[str, Any] | None = None,
    sort: bool = True,
    sort_column: str = "ts",
) -> pd.DataFrame:
    """Fetch via columnar protocol and assemble a typed DataFrame."""
    merged_settings = {**QUERY_SETTINGS, **(settings or {})}
    columnar_data, columns_with_types = client.execute(
        sql,
        params,
        columnar=True,
        with_column_types=True,
        settings=merged_settings,
    )
    if not columnar_data or not columnar_data[0]:
        return pd.DataFrame(columns=list(columns))
    names = [name for name, _ in columns_with_types]
    df = _build_frame(names, columnar_data)
    if sort:
        return _sort_by_ts(df, column=sort_column)
    return df.reset_index(drop=True)


def previous_complete_week_bounds(
    reference: pd.Timestamp | None = None,
) -> tuple[pd.Timestamp, pd.Timestamp]:
    """Return [Monday 00:00 UTC, next Monday 00:00 UTC) for the latest full Mon–Sun week."""
    ref = (reference or pd.Timestamp.now(tz="UTC")).tz_convert("UTC")
    this_week_monday = ref.normalize() - pd.Timedelta(days=ref.weekday())
    week_start = this_week_monday - pd.Timedelta(days=7)
    week_end = this_week_monday
    return week_start, week_end


def availability_from_df(df: pd.DataFrame, symbol: str) -> dict[str, Any]:
    """Derive row count and ranges from an already-loaded DataFrame."""
    if df.empty:
        return {
            "symbol": symbol,
            "row_count": 0,
            "ts_min": None,
            "ts_max": None,
            "ingestion_min": None,
            "ingestion_max": None,
        }
    return {
        "symbol": symbol,
        "row_count": len(df),
        "ts_min": df["ts"].min(),
        "ts_max": df["ts"].max(),
        "ingestion_min": df["ingestion_time"].min(),
        "ingestion_max": df["ingestion_time"].max(),
    }


def symbol_availability(
    client,
    symbol: str,
    *,
    database: str = DEFAULT_DATABASE,
    table: str = DEFAULT_TABLE,
) -> dict[str, Any]:
    """Return row count and ts/ingestion ranges for one symbol (metadata-only query)."""
    row_count, min_ts, max_ts, min_ingestion, max_ingestion = client.execute(
        f"""
        SELECT
            count(*) AS rows,
            min(ts) AS min_ts,
            max(ts) AS max_ts,
            min(ingestion_time) AS min_ingestion,
            max(ingestion_time) AS max_ingestion
        FROM {database}.{table}
        PREWHERE symbol = %(symbol)s
        """,
        {"symbol": symbol},
        settings=QUERY_SETTINGS,
    )[0]
    return {
        "symbol": symbol,
        "row_count": row_count,
        "ts_min": min_ts,
        "ts_max": max_ts,
        "ingestion_min": min_ingestion,
        "ingestion_max": max_ingestion,
    }


def load_bbo_df(
    client,
    *,
    database: str = DEFAULT_DATABASE,
    table: str = DEFAULT_TABLE,
    symbol: str | None = None,
    hours: int | None = 24,
    ts_start: pd.Timestamp | None = None,
    ts_end: pd.Timestamp | None = None,
    limit: int | None = 50_000,
    enrich: bool = True,
    sort: bool | None = None,
) -> pd.DataFrame:
    """Load BBO rows into a chronologically sorted DataFrame.

    Filter by quote ``ts`` when ``ts_start``/``ts_end`` are set; otherwise by
    ``ingestion_time`` over the last ``hours``.

    Performance notes:
    - ``PREWHERE`` on ``symbol`` / ``ts``
    - columnar fetch + numpy float assembly (no row-wise Python loops)
    - mid/spread/quote_age from table ALIAS columns when ``enrich=True``
    - numpy ``argsort`` for chronological ordering
    """
    params: dict[str, Any] = {}
    prewhere: list[str] = []
    where: list[str] = []

    if symbol is not None:
        prewhere.append("symbol = %(symbol)s")
        params["symbol"] = symbol
    if ts_start is not None and ts_end is not None:
        prewhere.append("ts >= %(ts_start)s")
        prewhere.append("ts < %(ts_end)s")
        params["ts_start"] = ts_start.to_pydatetime()
        params["ts_end"] = ts_end.to_pydatetime()
    elif hours is not None:
        where.append("ingestion_time >= now64(3) - INTERVAL %(hours)s HOUR")
        params["hours"] = int(hours)

    prewhere_sql = f"PREWHERE {' AND '.join(prewhere)}" if prewhere else ""
    where_sql = f"WHERE {' AND '.join(where)}" if where else ""
    limit_sql = "LIMIT %(limit)s" if limit is not None else ""
    if limit is not None:
        params["limit"] = int(limit)

    read_in_storage_order = (
        symbol is not None
        and ts_start is None
        and ts_end is None
        and hours is None
        and limit is None
    )
    if sort is None:
        sort = True

    if read_in_storage_order:
        order_sql = "ORDER BY platform, server, spread_profile, ts"
        fetch_settings = {"optimize_read_in_order": 1}
    else:
        order_sql = ""
        fetch_settings = {}

    select_sql = ENRICHED_SELECT_SQL if enrich else ", ".join(BBO_SELECT_COLUMNS)
    output_columns = BBO_LOAD_COLUMNS if enrich else BBO_SELECT_COLUMNS
    sql = f"""
        SELECT {select_sql}
        FROM {database}.{table}
        {prewhere_sql}
        {where_sql}
        {order_sql}
        {limit_sql}
    """
    return _fetch_dataframe(
        client,
        sql,
        params,
        columns=output_columns,
        settings=fetch_settings,
        sort=sort,
    )


def table_overview(client, database: str = DEFAULT_DATABASE, table: str = DEFAULT_TABLE) -> dict[str, Any]:
    """Return row count, symbol list, and ingestion time range."""
    row_count = client.execute(
        f"SELECT count(*) FROM {database}.{table} FINAL",
        settings=QUERY_SETTINGS,
    )[0][0]
    symbol_counts = client.execute(
        f"""
        SELECT symbol, count(*) AS n
        FROM {database}.{table}
        GROUP BY symbol
        ORDER BY n DESC
        """,
        settings=QUERY_SETTINGS,
    )
    time_range = client.execute(
        f"""
        SELECT min(ingestion_time), max(ingestion_time)
        FROM {database}.{table}
        """,
        settings=QUERY_SETTINGS,
    )[0]
    return {
        "row_count": row_count,
        "symbol_counts": symbol_counts,
        "ingestion_min": time_range[0],
        "ingestion_max": time_range[1],
    }


def daily_quote_stats(df: pd.DataFrame) -> pd.DataFrame:
    """Per-calendar-day first/last quote timestamps and row counts (UTC)."""
    if df.empty:
        return pd.DataFrame(columns=["first_ts", "last_ts", "rows", "dow"])
    ts = pd.to_datetime(df["ts"], utc=True)
    stats = (
        df.assign(_day=ts.dt.normalize())
        .groupby("_day", sort=True)
        .agg(first_ts=("ts", "min"), last_ts=("ts", "max"), rows=("ts", "count"))
    )
    stats.index.name = "day"
    stats["dow"] = stats.index.dayofweek
    return stats


def latest_trading_day_bounds(df: pd.DataFrame) -> tuple[pd.Timestamp, pd.Timestamp, str]:
    """Return [first quote, last quote] for the latest Mon–Fri day in *df*."""
    daily = daily_quote_stats(df)
    weekdays = daily[daily["dow"].between(0, 4)]
    if weekdays.empty:
        raise ValueError("no weekday quotes in dataframe")
    day = weekdays.index[-1]
    start = weekdays.loc[day, "first_ts"]
    end = weekdays.loc[day, "last_ts"]
    label = f"{day.date()} ({start.strftime('%H:%M')} → {end.strftime('%H:%M')} UTC)"
    return start, end, label


def latest_trading_week_bounds(df: pd.DataFrame) -> tuple[pd.Timestamp, pd.Timestamp, str]:
    """Return Mon open → Fri close using actual first/last quote timestamps in *df*."""
    daily = daily_quote_stats(df)
    weekdays = daily[daily["dow"].between(0, 4)]
    fridays = weekdays.index[weekdays["dow"] == 4]
    for friday in reversed(fridays.tolist()):
        monday = friday - pd.Timedelta(days=4)
        week_days = pd.date_range(monday, friday, freq="D", tz="UTC")
        if not all(day in weekdays.index for day in week_days):
            continue
        start = daily.loc[monday, "first_ts"]
        end = daily.loc[friday, "last_ts"]
        label = (
            f"{monday.date()} Mon {start.strftime('%H:%M')} UTC → "
            f"{friday.date()} Fri {end.strftime('%H:%M')} UTC"
        )
        return start, end, label
    raise ValueError("no complete Mon–Fri week found in dataframe")


def full_history_bounds(df: pd.DataFrame) -> tuple[pd.Timestamp, pd.Timestamp, str]:
    """Return min/max quote timestamps for all rows in *df*."""
    if df.empty:
        raise ValueError("empty dataframe")
    start = df["ts"].min()
    end = df["ts"].max()
    label = f"{start.date()} → {end.date()}"
    return start, end, label


def suggest_resample_rule(ts_start: pd.Timestamp, ts_end: pd.Timestamp) -> str:
    """Pick a pandas resample rule that keeps charts readable for the time span."""
    hours = (ts_end - ts_start).total_seconds() / 3600
    if hours <= 36:
        return "2min"
    if hours <= 24 * 7:
        return "15min"
    if hours <= 24 * 45:
        return "1h"
    return "4h"


def resample_bbo_chart(
    df: pd.DataFrame,
    *,
    ts_start: pd.Timestamp,
    ts_end: pd.Timestamp,
    spread_profile: str,
    rule: str,
) -> pd.DataFrame:
    """Filter a profile/window and resample bid/ask/mid/spread for plotting (tick source)."""
    window = df.loc[(df["ts"] >= ts_start) & (df["ts"] <= ts_end)]
    window = window.loc[window["spread_profile"] == spread_profile]
    if window.empty:
        return pd.DataFrame(columns=["bid_price", "ask_price", "mid", "spread_bps"])
    return (
        window.set_index("ts")
        .sort_index()
        .resample(rule)
        .agg(
            {
                "bid_price": "last",
                "ask_price": "last",
                "mid": "last",
                "spread_bps": "median",
            }
        )
        .dropna(how="all")
    )


def enrich_bbo(df: pd.DataFrame) -> pd.DataFrame:
    """Add mid/spread columns when they were not loaded via ``enrich=True``."""
    if {"mid", "spread_abs", "spread_bps"}.issubset(df.columns):
        return df
    bid = _as_float64(df["bid_price"].to_numpy())
    ask = _as_float64(df["ask_price"].to_numpy())
    mid = (bid + ask) * 0.5
    spread_abs = ask - bid
    with np.errstate(divide="ignore", invalid="ignore"):
        spread_bps = np.divide(spread_abs, mid, out=np.full_like(spread_abs, np.nan), where=mid != 0)
        spread_bps *= 10_000
    out = df.copy()
    out["bid_price"] = bid
    out["ask_price"] = ask
    out["mid"] = mid
    out["spread_abs"] = spread_abs
    out["spread_bps"] = spread_bps
    return out
