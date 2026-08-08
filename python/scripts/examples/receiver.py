"""
Swissquote BBO receiver example (Cython SBE decode).

Requires swissquote-feed running (systemd or manual):
    ~/.local/bin/swissquote-feed --host 127.0.0.1 --port 5559
"""

from __future__ import annotations

import argparse

from swissquote import BboSubscriber, flatten_bbo_rows


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Swissquote BBO receiver (ZMQ SUB, SBE)")
    parser.add_argument("--endpoint", default="tcp://127.0.0.1:5559")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    count = 0
    with BboSubscriber(args.endpoint) as sub:
        print(f"connected to {sub.endpoint}")
        print("waiting for SBE messages (Ctrl+C to stop)")
        try:
            while True:
                frame = sub.recv_frame(timeout_ms=1000)
                if frame is None:
                    continue
                rows = flatten_bbo_rows(frame)
                if not rows:
                    continue
                count += 1
                symbol = rows[0].symbol
                venue_count = len({(r.platform, r.server) for r in rows})
                print(f"[{count}] {symbol} venues={venue_count} profiles={len(rows)}")
        except KeyboardInterrupt:
            print(f"stopped ({count} messages)")


if __name__ == "__main__":
    main()
