#!/usr/bin/env python3
"""Migrate an existing service config to the Unix-domain IPC path."""

from __future__ import annotations

import json
import os
import re
import shutil
import stat
import sys
import tempfile
from pathlib import Path


def migrate(config_path: Path, socket_path: str) -> bool:
    config_path = config_path.resolve(strict=True)
    original = config_path.read_text(encoding="utf-8")
    lines = original.splitlines()

    without_legacy: list[str] = []
    in_legacy_transport = False
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("[") and stripped.endswith("]"):
            in_legacy_transport = stripped == "[zmq]"
            if in_legacy_transport:
                continue
        if not in_legacy_transport:
            without_legacy.append(line)

    output: list[str] = []
    in_ipc = False
    has_ipc = False
    has_socket_path = False
    for line in without_legacy:
        stripped = line.strip()
        if stripped.startswith("[") and stripped.endswith("]"):
            if in_ipc and not has_socket_path:
                output.append(f"socket_path = {json.dumps(socket_path)}")
                has_socket_path = True
            in_ipc = stripped == "[ipc]"
            has_ipc = has_ipc or in_ipc
            output.append(line)
            continue
        if in_ipc and re.match(r"^\s*socket_path\s*=", line):
            if not has_socket_path:
                output.append(f"socket_path = {json.dumps(socket_path)}")
                has_socket_path = True
            continue
        output.append(line)

    if in_ipc and not has_socket_path:
        output.append(f"socket_path = {json.dumps(socket_path)}")
    if not has_ipc:
        output = ["[ipc]", f"socket_path = {json.dumps(socket_path)}", "", *output]

    updated = "\n".join(output) + "\n"
    if updated == original:
        return False

    backup = config_path.with_name(config_path.name + ".pre-unix-ipc")
    if not backup.exists():
        shutil.copy2(config_path, backup)

    info = config_path.stat()
    fd, temporary_name = tempfile.mkstemp(prefix=f".{config_path.name}.", dir=config_path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as temporary:
            temporary.write(updated)
            temporary.flush()
            os.fsync(temporary.fileno())
        os.chmod(temporary_name, stat.S_IMODE(info.st_mode))
        if os.geteuid() == 0:
            os.chown(temporary_name, info.st_uid, info.st_gid)
        os.replace(temporary_name, config_path)
    except BaseException:
        try:
            os.unlink(temporary_name)
        except FileNotFoundError:
            pass
        raise
    return True


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: migrate_ipc_config.py SERVICE_CONFIG SOCKET_PATH", file=sys.stderr)
        return 2
    config_path = Path(sys.argv[1])
    socket_path = sys.argv[2]
    if not config_path.is_file():
        print(f"service config does not exist: {config_path}", file=sys.stderr)
        return 1
    if not socket_path.startswith("/") or any(ch.isspace() for ch in socket_path):
        print("socket path must be absolute and contain no whitespace", file=sys.stderr)
        return 1
    if migrate(config_path, socket_path):
        print(f"migrated {config_path} to Unix socket {socket_path}")
    else:
        print(f"{config_path} already uses Unix socket {socket_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
