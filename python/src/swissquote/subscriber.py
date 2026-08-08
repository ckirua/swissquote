"""ZMQ subscriber for the C++ BBO feed."""

from __future__ import annotations

import zmq


class BboSubscriber:
    """Subscribe to raw SBE frames from swissquote-feed."""

    def __init__(self, endpoint: str = "tcp://127.0.0.1:5559") -> None:
        self._endpoint = endpoint
        self._ctx = zmq.Context.instance()
        self._socket = self._ctx.socket(zmq.SUB)
        self._socket.setsockopt(zmq.LINGER, 0)
        self._socket.setsockopt(zmq.SUBSCRIBE, b"")
        self._open = False

    @property
    def endpoint(self) -> str:
        return self._endpoint

    def start(self) -> None:
        if not self._open:
            self._socket.connect(self._endpoint)
            self._open = True

    def recv_frame(self, timeout_ms: int = 1000) -> bytes | None:
        if not self._open:
            self.start()
        if self._socket.poll(timeout_ms) == 0:
            return None
        return self._socket.recv()

    def close(self) -> None:
        if self._open:
            self._socket.close(linger=0)
            self._open = False

    def __enter__(self) -> BboSubscriber:
        self.start()
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()


__all__: list[str] = ["BboSubscriber"]
