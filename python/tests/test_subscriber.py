"""Unix-domain feed subscriber tests."""

from __future__ import annotations

import socket
import threading

from swissquote import BboSubscriber


def test_subscriber_timeout_frame_and_reconnect(tmp_path) -> None:
    socket_path = str(tmp_path / "feed.sock")
    listener = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    listener.bind(socket_path)
    listener.listen()
    release_first = threading.Event()
    first_closed = threading.Event()
    frame = b"first-sbe-frame"
    reconnect_frame = b"second-sbe-frame"

    def serve() -> None:
        connection, _ = listener.accept()
        with connection:
            assert release_first.wait(timeout=2)
            connection.sendall(frame)
        first_closed.set()
        connection, _ = listener.accept()
        with connection:
            connection.sendall(reconnect_frame)
        listener.close()

    server = threading.Thread(target=serve, daemon=True)
    server.start()
    subscriber = BboSubscriber(socket_path)
    try:
        subscriber.start()
        assert subscriber.socket_path == socket_path
        assert subscriber.recv_frame(timeout_ms=10) is None
        release_first.set()
        assert subscriber.recv_frame(timeout_ms=1000) == frame
        assert first_closed.wait(timeout=2)
        assert subscriber.recv_frame(timeout_ms=1000) is None
        assert subscriber.recv_frame(timeout_ms=1000) == reconnect_frame
    finally:
        subscriber.close()
        listener.close()
        server.join(timeout=2)
