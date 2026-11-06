#include "host_tcp.h"

#include <cstdio>
#include <cstring>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <algorithm>

#if defined(_WIN32)
#error "FirmwareHost TCP stub targets POSIX (macOS/Linux)"
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

extern "C" void hostBleOnClientConnect();
extern "C" void hostBleOnClientDisconnect();

namespace {

std::atomic<bool> gRun{false};
int gListenFd = -1;
int gClientFd = -1;
std::mutex gClientMu;
std::vector<uint8_t> gRxAcc;
bool gClientAnnounced = false;

void setNonBlock(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void closeClient() {
  bool had = false;
  {
    std::lock_guard<std::mutex> lock(gClientMu);
    if (gClientFd >= 0) {
      ::close(gClientFd);
      gClientFd = -1;
      had = gClientAnnounced;
      gClientAnnounced = false;
    }
    gRxAcc.clear();
  }
  if (had) hostBleOnClientDisconnect();
}

void handlePayload(const uint8_t *data, size_t len) {
  if (!data || len == 0 || len > HOST_TCP_MAX_PAYLOAD) return;
  hostRxEnqueue(data, len);
}

void feedBytes(const uint8_t *data, size_t n) {
  gRxAcc.insert(gRxAcc.end(), data, data + n);
  for (;;) {
    if (gRxAcc.size() < 4) return;
    uint32_t len = (uint32_t)gRxAcc[0] | ((uint32_t)gRxAcc[1] << 8) |
                   ((uint32_t)gRxAcc[2] << 16) | ((uint32_t)gRxAcc[3] << 24);
    if (len > HOST_TCP_MAX_PAYLOAD) {
      std::fprintf(stderr, "HOST_SIM TCP: bad frame length %u — dropping client\n", len);
      closeClient();
      return;
    }
    if (gRxAcc.size() < 4u + len) return;
    handlePayload(gRxAcc.data() + 4, len);
    gRxAcc.erase(gRxAcc.begin(), gRxAcc.begin() + 4 + (std::ptrdiff_t)len);
  }
}

} // namespace

bool hostTcpStart(const char *host, uint16_t port) {
  if (gRun.load()) return true;
  gListenFd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (gListenFd < 0) {
    std::perror("HOST_SIM TCP socket");
    return false;
  }
  int yes = 1;
  setsockopt(gListenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (!host || !*host) host = "127.0.0.1";
  if (::inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
    std::fprintf(stderr, "HOST_SIM TCP: bad host %s\n", host);
    ::close(gListenFd);
    gListenFd = -1;
    return false;
  }
  if (::bind(gListenFd, (sockaddr *)&addr, sizeof(addr)) < 0) {
    std::perror("HOST_SIM TCP bind");
    ::close(gListenFd);
    gListenFd = -1;
    return false;
  }
  if (::listen(gListenFd, 4) < 0) {
    std::perror("HOST_SIM TCP listen");
    ::close(gListenFd);
    gListenFd = -1;
    return false;
  }
  setNonBlock(gListenFd);
  gRun = true;
  std::printf("HOST_SIM TCP listening on %s:%u\n", host, (unsigned)port);
  return true;
}

void hostTcpStop() {
  gRun = false;
  closeClient();
  if (gListenFd >= 0) {
    ::close(gListenFd);
    gListenFd = -1;
  }
}

void hostTcpPoll() {
  if (!gRun.load() || gListenFd < 0) return;

  {
    std::lock_guard<std::mutex> lock(gClientMu);
    if (gClientFd < 0) {
      sockaddr_in peer{};
      socklen_t plen = sizeof(peer);
      int c = ::accept(gListenFd, (sockaddr *)&peer, &plen);
      if (c >= 0) {
        setNonBlock(c);
        gClientFd = c;
        gRxAcc.clear();
        gClientAnnounced = true;
        char ip[64];
        ::inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
        std::printf("HOST_SIM TCP client %s:%u\n", ip, (unsigned)ntohs(peer.sin_port));
        hostBleOnClientConnect();
      }
    }
  }

  int fd = -1;
  {
    std::lock_guard<std::mutex> lock(gClientMu);
    fd = gClientFd;
  }
  if (fd < 0) return;

  uint8_t buf[1024];
  for (;;) {
    ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
    if (n > 0) {
      feedBytes(buf, (size_t)n);
      continue;
    }
    if (n == 0) {
      std::printf("HOST_SIM TCP client closed\n");
      closeClient();
      break;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) break;
    if (errno == EINTR) continue;
    std::perror("HOST_SIM TCP recv");
    closeClient();
    break;
  }
}

bool hostTcpSend(const uint8_t *data, size_t len) {
  if (!data || len == 0 || len > HOST_TCP_MAX_PAYLOAD) return false;
  std::lock_guard<std::mutex> lock(gClientMu);
  if (gClientFd < 0) return false;
  uint8_t hdr[4] = {
      (uint8_t)(len & 0xff),
      (uint8_t)((len >> 8) & 0xff),
      (uint8_t)((len >> 16) & 0xff),
      (uint8_t)((len >> 24) & 0xff),
  };
  if (::send(gClientFd, hdr, 4, 0) != 4) return false;
  return ::send(gClientFd, data, len, 0) == (ssize_t)len;
}
