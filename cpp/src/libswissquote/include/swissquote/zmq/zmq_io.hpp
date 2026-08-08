#pragma once

#include <cstddef>

struct zmq_msg_t;

namespace swissquote::zmq::io {

// Returns 0 on success, 1 on EAGAIN, -1 on other error.
int try_send_bytes(void* socket, const char* data, std::size_t data_len) noexcept;

// msg must be initialized by caller; closed on error paths inside.
int try_recv_frame(void* socket, zmq_msg_t* msg) noexcept;

void poll_one(void* socket, int events, int timeout_ms) noexcept;

} // namespace swissquote::zmq::io
