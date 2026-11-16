#include "componeng/net/network.hpp"

#include "componeng/net/udp_socket.hpp"

#include <utility>

namespace componeng::net {

Network::Network() : m_binding(std::make_shared<WorldBinding>()) {
}

Network::~Network() = default;
Network::Network(Network &&) noexcept = default;
Network &Network::operator=(Network &&) noexcept = default;

bool Network::open(uint16_t port, Address::Family family,
                   const TransportConfig &config) {
  auto socket = std::make_unique<UdpSocket>();
  if (!socket->open(Address::any(port, family))) {
    return false;
  }
  return open(std::move(socket), config);
}

bool Network::open(std::unique_ptr<IDatagramSocket> socket,
                   const TransportConfig &config) {
  close();
  if (!socket || !socket->isOpen()) {
    return false;
  }
  m_socket = std::move(socket);
  m_transport = std::make_unique<Transport>(*m_socket, config);
  return true;
}

void Network::close() {
  // The transport says goodbye through the socket, so it goes first.
  m_transport.reset();
  m_socket.reset();
}

Address Network::localAddress() const {
  return m_socket ? m_socket->localAddress() : Address();
}

void Network::setAcceptIncoming(bool accept) {
  if (m_transport) {
    m_transport->setAcceptIncoming(accept);
  }
}

bool Network::connect(const Address &peer) {
  return m_transport && m_transport->connect(peer);
}

bool Network::connect(const std::string &text) {
  Address peer;
  return Address::parse(text, peer) && connect(peer);
}

void Network::disconnect(const Address &peer) {
  if (m_transport) {
    m_transport->disconnect(peer);
  }
}

bool Network::isConnected(const Address &peer) const {
  return m_transport && m_transport->isConnected(peer);
}

std::vector<Address> Network::peers() const {
  return m_transport ? m_transport->connectedPeers() : std::vector<Address>();
}

ConnectionStats Network::stats(const Address &peer) const {
  return m_transport ? m_transport->stats(peer) : ConnectionStats();
}

void Network::onPeerConnected(std::function<void(const Address &)> handler) {
  m_onConnected.push_back(std::move(handler));
}

void Network::onPeerDisconnected(
    std::function<void(const Address &, DisconnectReason)> handler) {
  m_onDisconnected.push_back(std::move(handler));
}

void Network::onConnectFailed(
    std::function<void(const Address &, DisconnectReason)> handler) {
  m_onConnectFailed.push_back(std::move(handler));
}

void Network::update(double now) {
  if (!m_transport) {
    return;
  }
  m_transport->update(now);
  // A handler may close() the network, so look again before each event.
  Event event;
  while (m_transport && m_transport->pollEvent(event)) {
    dispatch(event);
  }
}

void Network::dispatch(const Event &event) {
  switch (event.type) {
    case Event::Type::Connected:
      for (size_t i = 0; i < m_onConnected.size(); ++i) {
        m_onConnected[i](event.peer);
      }
      if (m_binding->world) {
        m_binding->world->emitEvent(events::PeerConnectedEvent{event.peer});
      }
      break;

    case Event::Type::Disconnected:
      for (size_t i = 0; i < m_onDisconnected.size(); ++i) {
        m_onDisconnected[i](event.peer, event.reason);
      }
      if (m_binding->world) {
        m_binding->world->emitEvent(
            events::PeerDisconnectedEvent{event.peer, event.reason});
      }
      break;

    case Event::Type::ConnectFailed:
      for (size_t i = 0; i < m_onConnectFailed.size(); ++i) {
        m_onConnectFailed[i](event.peer, event.reason);
      }
      if (m_binding->world) {
        m_binding->world->emitEvent(
            events::ConnectFailedEvent{event.peer, event.reason});
      }
      break;

    case Event::Type::Message:
      if (m_registry.decode(event.peer, event.channel, event.data.data(),
                            event.data.size()) !=
          MessageRegistry::DecodeResult::Ok) {
        ++m_rejectedMessages;
      }
      break;
  }
}

} // namespace componeng::net
