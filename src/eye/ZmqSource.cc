#include "ZmqSource.h"

#include <algorithm>
#include <cstring>
#include <iostream>

ZmqSource::ZmqSource(const std::string &uri)
    : context(nullptr), socket(nullptr), ready(false) {
  context = zmq_ctx_new();
  if (!context) {
    std::cerr << "Failed to create ZMQ context" << std::endl;
    return;
  }

  socket = zmq_socket(context, ZMQ_SUB);
  if (!socket) {
    std::cerr << "Failed to create ZMQ socket" << std::endl;
    zmq_ctx_term(context);
    context = nullptr;
    return;
  }

  int rc = zmq_setsockopt(socket, ZMQ_SUBSCRIBE, "", 0);
  if (rc != 0) {
    std::cerr << "Failed to subscribe to ZMQ socket" << std::endl;
    zmq_close(socket);
    zmq_ctx_term(context);
    socket = nullptr;
    context = nullptr;
    return;
  }

  if (zmq_connect(socket, uri.c_str()) != 0) {
    std::cerr << "Failed to connect to ZMQ URI: " << uri << std::endl;
    zmq_close(socket);
    zmq_ctx_term(context);
    socket = nullptr;
    context = nullptr;
    return;
  }

  ready = true;
}

ZmqSource::~ZmqSource() {
  if (socket) {
    zmq_close(socket);
    socket = nullptr;
  }
  if (context) {
    zmq_ctx_term(context);
    context = nullptr;
  }
}

int ZmqSource::recv(cf_t *data[SRSLTE_MAX_PORTS], uint32_t nsamples) {
  (void)data; // handled explicitly below
  const size_t required_bytes = static_cast<size_t>(nsamples) * sizeof(cf_t);
  size_t copied = 0;

  // Ensure buffer exists
  if (!data[0]) {
    return SRSLTE_ERROR_INVALID_INPUTS;
  }

  uint8_t *out_ptr = reinterpret_cast<uint8_t *>(data[0]);

  // Use any leftover bytes from previous messages first
  if (!leftover.empty()) {
    size_t to_copy = std::min(required_bytes, leftover.size());
    std::memcpy(out_ptr, leftover.data(), to_copy);
    copied += to_copy;
    if (to_copy < leftover.size()) {
      leftover.erase(leftover.begin(), leftover.begin() + static_cast<long>(to_copy));
      return static_cast<int>(copied / sizeof(cf_t));
    }
    leftover.clear();
  }

  while (copied < required_bytes) {
    zmq_msg_t msg;
    zmq_msg_init(&msg);
    int bytes = zmq_msg_recv(&msg, socket, 0);
    if (bytes < 0) {
      zmq_msg_close(&msg);
      return SRSLTE_ERROR;
    }

    size_t available = static_cast<size_t>(bytes);
    size_t remaining = required_bytes - copied;
    size_t to_copy = std::min(available, remaining);
    std::memcpy(out_ptr + copied, zmq_msg_data(&msg), to_copy);
    copied += to_copy;

    if (to_copy < available) {
      uint8_t *start = static_cast<uint8_t *>(zmq_msg_data(&msg)) + to_copy;
      leftover.assign(start, start + (available - to_copy));
    }
    zmq_msg_close(&msg);
  }

  return static_cast<int>(copied / sizeof(cf_t));
}

int zmq_source_recv_wrapper(void *h, cf_t *data[SRSLTE_MAX_PORTS], uint32_t nsamples, srslte_timestamp_t *t) {
  (void)t;
  if (h == nullptr) {
    return SRSLTE_ERROR_INVALID_INPUTS;
  }
  auto *source = reinterpret_cast<ZmqSource *>(h);
  return source->recv(data, nsamples);
}
