#!/usr/bin/env python3
"""Audit and optionally delete corrupt rows from swissquote.bbo.

Always run with ``--dry-run`` first (default). Use ``--apply --yes`` to mutate.

Example:
    source ~/.venvs/swissquote/bin/activate
    python scripts/cleanup_bbo.py
    python scripts/cleanup_bbo.py --apply --yes
    python scripts/cleanup_bbo.py --apply --yes --optimize
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass
from typing import Final

from swissquote.clickhouse import DEFAULT_DATABASE, DEFAULT_TABLE, ClickHouseConfig, create_client

VALID_PLATFORMS_SQL: Final[str] = "('AT', 'SwissquoteCapitalMarkets', 'SwissquoteLtd')"
VALID_SERVERS_SQL: Final[str] = "('AT', 'Live5', 'Live7')"


@dataclass(frozen=True)
class CleanupRule:
    name: str
    where_sql: str
    description: str


RULES: tuple[CleanupRule, ...] = (
    CleanupRule(
        "bad_quote_ts",
        "toYear(ts) < 2020",
        "Quote timestamp before 2020 (mis-decoded venueTsMs)",
    ),
    CleanupRule(
        "bad_platform",
        f"platform NOT IN {VALID_PLATFORMS_SQL}",
        "Platform not in known feed topos (e.g. apitalMarkets)",
    ),
    CleanupRule(
        "bad_server",
        f"server NOT IN {VALID_SERVERS_SQL}",
        "Server not in known feed topos",
    ),
    CleanupRule(
        "crossed_book",
        "bid_price > ask_price",
        "Crossed book",
    ),
    CleanupRule(
        "xau_family_low_bid",
        "(symbol LIKE 'XAU/%' OR symbol LIKE 'XPD/%' OR symbol LIKE 'XPT/%') AND bid_price < 1000",
        "Precious-metal symbols with FX-scale prices",
    ),
    CleanupRule(
        "xag_family_low_bid",
        "symbol LIKE 'XAG/%' AND bid_price < 1",
        "Silver symbols with near-zero prices",
    ),
    CleanupRule(
        "pre_continuous_feed",
        "ts < '2026-07-06 00:00:00'",
        "Sparse Apr/May snapshot era before continuous feed (Jul 6+)",
    ),
)


def _count(client, where_sql: str) -> int:
    return int(
        client.execute(
            f"SELECT count() FROM {DEFAULT_DATABASE}.{DEFAULT_TABLE} WHERE {where_sql}",
            settings={"max_threads": 0},
        )[0][0]
    )


def _print_rule_counts(client, rules: tuple[CleanupRule, ...]) -> int:
    total_table = _count(client, "1")
    print(f"Table rows: {total_table:,}\n")
    print(f"{'Rule':<22} {'Rows':>12}  Description")
    print("-" * 72)
    union_parts: list[str] = []
    for rule in rules:
        n = _count(client, rule.where_sql)
        print(f"{rule.name:<22} {n:>12,}  {rule.description}")
        if n:
            union_parts.append(f"({rule.where_sql})")
    print()
    if not union_parts:
        print("Union (any rule): 0")
        return 0
    union_sql = " OR ".join(union_parts)
    union_count = _count(client, union_sql)
    print(f"Union (any rule): {union_count:,} ({100.0 * union_count / max(total_table, 1):.4f}%)")
    return union_count


def _apply_deletes(client, rules: tuple[CleanupRule, ...]) -> None:
    for rule in rules:
        n = _count(client, rule.where_sql)
        if n == 0:
            print(f"skip {rule.name}: 0 rows")
            continue
        print(f"DELETE {rule.name}: {n:,} rows …")
        client.execute(
            f"ALTER TABLE {DEFAULT_DATABASE}.{DEFAULT_TABLE} DELETE WHERE {rule.where_sql}",
            settings={"max_threads": 0},
        )


def _optimize(client) -> None:
    print(f"OPTIMIZE TABLE {DEFAULT_DATABASE}.{DEFAULT_TABLE} FINAL …")
    client.execute(
        f"OPTIMIZE TABLE {DEFAULT_DATABASE}.{DEFAULT_TABLE} FINAL",
        settings={"max_threads": 0},
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--apply",
        action="store_true",
        help="Run ALTER TABLE … DELETE for each rule (default: dry-run counts only)",
    )
    parser.add_argument(
        "--yes",
        action="store_true",
        help="Required with --apply to confirm destructive mutations",
    )
    parser.add_argument(
        "--optimize",
        action="store_true",
        help="Run OPTIMIZE TABLE … FINAL after deletes (use with --apply)",
    )
    parser.add_argument(
        "--rule",
        action="append",
        choices=[r.name for r in RULES],
        help="Limit to one or more rules (repeatable)",
    )
    args = parser.parse_args(argv)

    rules = RULES
    if args.rule:
        selected = set(args.rule)
        rules = tuple(r for r in RULES if r.name in selected)

    cfg = ClickHouseConfig.from_env()
    client = create_client(cfg)

    print(f"Target: {DEFAULT_DATABASE}.{DEFAULT_TABLE} @ {cfg.host}:{cfg.port}")
    print("Mode:   APPLY" if args.apply else "Mode:   DRY-RUN\n")

    union_count = _print_rule_counts(client, rules)
    if not args.apply:
        if union_count:
            print("\nRe-run with --apply --yes to delete. Add --optimize to compact parts.")
        return 0

    if not args.yes:
        print("Refusing --apply without --yes", file=sys.stderr)
        return 2
    if union_count == 0:
        print("Nothing to delete.")
        return 0

    _apply_deletes(client, rules)
    print(
        "\nMutations queued. Check progress:\n"
        f"  SELECT * FROM system.mutations "
        f"WHERE database = '{DEFAULT_DATABASE}' AND table = '{DEFAULT_TABLE}' AND is_done = 0"
    )
    if args.optimize:
        print("\nNote: wait for mutations to finish before OPTIMIZE FINAL.")
        _optimize(client)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
