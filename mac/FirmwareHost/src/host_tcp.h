#pragma once
#include <cstdint>
#include <cstddef>

#ifndef HOST_TCP_PORT
#define HOST_TCP_PORT 17875
#endif
#ifndef HOST_TCP_MAX_PAYLOAD
#define HOST_TCP_MAX_PAYLOAD 500
#endif

// RoundSim-compatible framing: [u32 LE length][payload], max payload 500.
// Accepted payloads are passed to hostRxEnqueue (defined by firmware bundle).

extern "C" void hostRxEnqueue(const uint8_t *data, size_t len);

bool hostTcpStart(const char *host = "127.0.0.1", uint16_t port = HOST_TCP_PORT);
void hostTcpStop();
void hostTcpPoll(); // non-blocking accept/read; call from host loop
bool hostTcpSend(const uint8_t *data, size_t len); // notify → connected client(s)
