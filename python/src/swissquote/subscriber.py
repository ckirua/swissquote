"""Unix-domain SOCK_SEQPACKET subscriber for the C++ BBO feed."""

from __future__ import annotations

import select
import socket

_MAX_FRAME_SIZE = 65_536


class BboSubscriber:
    """Receive raw SBE frames from swissquote-feed over a Unix socket."""

    def __init__(self, socket_path: str = "/run/swissquote/feed.sock") -> None:
        self._socket_path = socket_path
        self._socket: socket.socket | None = None

    @property
    def socket_path(self) -> str:
        return self._socket_path

    def start(self) -> None:
        if self._socket is not None:
            return
        client = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        try:
            client.connect(self._socket_path)
        except OSError:
            client.close()
            raise
        self._socket = client

    def recv_frame(self, timeout_ms: int = 1000) -> bytes | None:
        if self._socket is None:
            try:
                self.start()
            except OSError:
                return None
        assert self._socket is not None
        readable, _, _ = select.select([self._socket], [], [], timeout_ms / 1000)
        if not readable:
            return None
        try:
            frame = self._socket.recv(_MAX_FRAME_SIZE)
        except OSError:
            self.close()
            return None
        if not frame:
            self.close()
            return None
        return frame

    def close(self) -> None:
        if self._socket is not None:
            self._socket.close()
            self._socket = None

    def __enter__(self) -> BboSubscriber:
        self.start()
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()


__all__: list[str] = ["BboSubscriber"]


