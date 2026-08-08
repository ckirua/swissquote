#include "swissquote/zmq/zmq_io.hpp"

#include <cerrno>
#include <cstring>

#include <zmq.h>

namespace swissquote::zmq::io {

int try_send_bytes(void* socket, const char* data, std::size_t data_len) noexcept {
    zmq_msg_t msg;
    if (zmq_msg_init_size(&msg, data_len) != 0) {
        return -1;
    }
    if (data_len > 0) {
        std::memcpy(zmq_msg_data(&msg), data, data_len);
    }
    const int rc = zmq_msg_send(&msg, socket, ZMQ_DONTWAIT);
    zmq_msg_close(&msg);
    if (rc < 0) {
        if (errno == EAGAIN) {
            return 1;
        }
        return -1;
    }
    return 0;
}

int try_recv_frame(void* socket, zmq_msg_t* msg) noexcept {
    const int rc = zmq_msg_recv(msg, socket, ZMQ_DONTWAIT);
    if (rc < 0) {
        zmq_msg_close(msg);
        if (errno == EAGAIN) {
            return 1;
        }
        return -1;
    }
    return 0;
}

void poll_one(void* socket, int events, int timeout_ms) noexcept {
    zmq_pollitem_t item{};
    item.socket = socket;
    item.events = static_cast<short>(events);
    zmq_poll(&item, 1, timeout_ms);
}

} // namespace swissquote::zmq::io
