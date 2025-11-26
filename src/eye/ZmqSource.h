#pragma once

#include <cstdint>
#include <string>
#include <vector>

#if __has_include("srslte/srslte.h")
#include "srslte/srslte.h"
#elif __has_include("srsran/srsran.h")
#include "srsran/srsran.h"
#else
#error "Neither srslte nor srsran headers are available"
#endif

extern "C" {
#include <zmq.h>
}

class ZmqSource {
public:
  explicit ZmqSource(const std::string &uri);
  ~ZmqSource();

  bool isReady() const { return ready; }
  int recv(cf_t *data[SRSLTE_MAX_PORTS], uint32_t nsamples);

private:
  void *context;
  void *socket;
  std::vector<uint8_t> leftover;
  bool ready;
};

int zmq_source_recv_wrapper(void *h, cf_t *data[SRSLTE_MAX_PORTS], uint32_t nsamples, srslte_timestamp_t *t);
