from __future__ import annotations

from typing import Any, Final

from swissquote.clickhouse import ClickHouseConfig, create_client

_NATIVE_PORT_HINT = (
    "This script uses clickhouse_driver (native TCP, default port 9000). "
    "Connection refused usually means port 9000 is closed on the firewall or "
    "ClickHouse is not listening on the public interface. "
    "Options: open/forward TCP 9000, or SSH tunnel (e.g. "
    "`ssh -L 9000:127.0.0.1:9000 user@host` and set CLICKHOUSE_HOST=127.0.0.1). "
    "HTTP-only access on 8123 needs a different client, not clickhouse_driver."
)

SHOW_DATABASES_QUERY: Final[str] = "SHOW DATABASES"
DISK_USAGE_QUERY: Final[str] = (
    "SELECT total_space, free_space FROM system.disks WHERE name = 'default'"
)
EXCLUDE_DATABASES: Final[frozenset[str]] = frozenset(
    {"default", "information_schema", "system", "information"}
)
TIME_COLUMN_CANDIDATES: Final[tuple[str, ...]] = (
    "ts",
    "event_time",
    "timestamp",
    "ingestion_time",
    "created_at",
    "updated_at",
)


def _format_size(size_bytes: int) -> str:
    """Format bytes to human readable size."""
    for unit in ("B", "KB", "MB", "GB", "TB"):
        if size_bytes < 1024:
            return f"{size_bytes:.1f} {unit}"
        size_bytes /= 1024
    return f"{size_bytes:.1f} PB"


def _is_time_type(column_type: str) -> bool:
    upper = column_type.upper()
    return upper.startswith(("DATETIME", "DATE"))


def _resolve_time_column(client: Any, database: str, table: str) -> str | None:
    rows = client.execute(
        """
        SELECT name, type
        FROM system.columns
        WHERE database = %(database)s AND table = %(table)s
        ORDER BY position
        """,
        {"database": database, "table": table},
    )
    by_name = {name: column_type for name, column_type in rows}
    for candidate in TIME_COLUMN_CANDIDATES:
        column_type = by_name.get(candidate)
        if column_type is not None and _is_time_type(column_type):
            return candidate
    for name, column_type in rows:
        if _is_time_type(column_type):
            return name
    return None


def _table_time_range(
    client: Any, database: str, table: str
) -> tuple[object, object] | None:
    column = _resolve_time_column(client, database, table)
    if column is None:
        return None
    min_value, max_value = client.execute(
        f"SELECT min(`{column}`), max(`{column}`) FROM `{database}`.`{table}`"
    )[0]
    return min_value, max_value


def _print_table_status(client: Any, database: str, table: str) -> None:
    rows = client.execute(f"SELECT count(*) FROM `{database}`.`{table}`")[0][0]
    size_bytes = client.execute(
        """
        SELECT sum(bytes_on_disk) FROM system.parts
        WHERE database = %(database)s AND table = %(table)s AND active
        """,
        {"database": database, "table": table},
    )[0][0] or 0
    size_str = _format_size(size_bytes)
    if rows == 0:
        print(f"  {table}: empty | {size_str}")
        return

    time_range = _table_time_range(client, database, table)
    if time_range is None:
        print(f"  {table}: {rows:,} rows | {size_str} | (no time column)")
        return

    min_date, max_date = time_range
    print(f"  {table}: {rows:,} rows | {size_str} | {min_date} → {max_date}")


def main() -> None:
    try:
        client = create_client(ClickHouseConfig.from_env())
    except Exception as e:
        print(f"Error connecting to ClickHouse: {e}")
        err_name = type(e).__name__
        msg = str(e).lower()
        if (
            err_name == "ConnectionRefusedError"
            or "connection refused" in msg
            or "code: 210" in msg
        ):
            print(_NATIVE_PORT_HINT)
        return

    try:
        total_space, free_space = client.execute(DISK_USAGE_QUERY)[0]
        used_space = total_space - free_space
        pct_used = (used_space / total_space) * 100 if total_space > 0 else 0
        print(
            f"Disk: {_format_size(used_space)} / {_format_size(total_space)} ({pct_used:.1f}% used)"
        )

        all_databases = client.execute(SHOW_DATABASES_QUERY)
        user_databases = [
            db[0] for db in all_databases if db[0].lower() not in EXCLUDE_DATABASES
        ]
        for db in user_databases:
            tables = [t[0] for t in client.execute(f"SHOW TABLES FROM `{db}`")]
            print(f"\nDatabase: {db}")
            for table in tables:
                try:
                    _print_table_status(client, db, table)
                except Exception as e:
                    print(f"  {table}: error — {e}")
    except Exception as e:
        print(f"Error fetching ClickHouse status: {e}")


if __name__ == "__main__":
    main()
