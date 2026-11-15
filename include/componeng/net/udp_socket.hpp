#pragma once

#include "componeng/net/address.hpp"
#include "componeng/net/idatagram_socket.hpp"

#include <cstdint>
#include <string>

namespace componeng::net {

// Non-blocking UDP socket over BSD sockets / Winsock; one address family per
// socket.
class UdpSocket : public IDatagramSocket {
public:
  UdpSocket() = default;
  ~UdpSocket() override;

  UdpSocket(const UdpSocket &) = delete;
  UdpSocket &operator=(const UdpSocket &) = delete;
  UdpSocket(UdpSocket &&other) noexcept;
  UdpSocket &operator=(UdpSocket &&other) noexcept;

  // Binds to `local`, closing any open socket first. False (see lastError()) if
  // it could not be bound, e.g. the port is taken.
  bool open(const Address &local);
  void close() override;
  bool isOpen() const override {
    return m_handle != kNoHandle;
  }

  IoStatus sendTo(const Address &to, const void *data, size_t size) override;
  IoStatus recvFrom(void *buffer, size_t capacity, size_t &received,
                    Address &from) override;
  bool waitReadable(int timeoutMs) override;

  Address localAddress() const override {
    return m_local;
  }
  const std::string &lastError() const override {
    return m_lastError;
  }

private:
  static constexpr intptr_t kNoHandle = -1;

  // int fd or SOCKET, widened to keep OS headers out of this file.
  intptr_t m_handle = kNoHandle;
  Address m_local;
  std::string m_lastError;
};

} // namespace componeng::net
