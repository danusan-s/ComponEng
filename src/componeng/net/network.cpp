#include "componeng/net/network.hpp"

#include "componeng/net/invite_code.hpp"
#include "componeng/net/udp_socket.hpp"

#include <thread>

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
  auto stunSocket = std::make_unique<StunSocket>(std::move(socket));
  m_stun = stunSocket.get();
  m_socket = std::move(stunSocket);
  m_config = config;
  m_transport = std::make_unique<Transport>(*m_socket, config);
  return true;
}

void Network::close() {
  // The transport says goodbye through the socket, so it goes first.
  m_transport.reset();
  m_stun = nullptr;
  m_socket.reset();
  m_resolve.reset();
  m_discoveryPending = false;
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
  updateNatDiscovery(now);
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

// ---- Reaching peers across the internet ----

std::vector<std::string> Network::defaultStunServers() {
  return {"stun.l.google.com:19302", "stun.cloudflare.com:3478"};
}

void Network::discoverPublicAddress(std::vector<std::string> servers) {
  if (!m_stun) {
    return;
  }
  if (servers.empty()) {
    servers = defaultStunServers();
  }
  m_discoveryPending = true;
  m_resolve.reset();
  const Address::Family family = m_socket->localAddress().family();

  // Numeric addresses need no lookup; only names do.
  std::vector<Address> numeric;
  std::vector<std::string> names;
  for (const std::string &text : servers) {
    Address address;
    if (Address::parse(text, address)) {
      if (address.family() == family) {
        numeric.push_back(address);
      }
    } else {
      names.push_back(text);
    }
  }
  if (names.empty()) {
    m_stun->discovery().start(numeric);
    return;
  }

  // Detached and touching only what it shares, so close() never waits for a
  // slow lookup.
  m_resolve = std::make_shared<ResolveJob>();
  m_resolve->servers = numeric;
  std::thread([job = m_resolve, names, family] {
    std::vector<Address> found;
    for (const std::string &text : names) {
      size_t colon = text.rfind(':');
      if (colon == std::string::npos || colon == 0) {
        continue;
      }
      uint32_t port = 0;
      try {
        port = static_cast<uint32_t>(std::stoul(text.substr(colon + 1)));
      } catch (...) {
        continue;
      }
      Address address;
      if (port != 0 && port <= 65535 &&
          Address::resolve(text.substr(0, colon), static_cast<uint16_t>(port),
                           family, address)) {
        found.push_back(address);
      }
    }
    std::lock_guard<std::mutex> lock(job->mutex);
    job->servers.insert(job->servers.end(), found.begin(), found.end());
    job->done = true;
  }).detach();
}

void Network::updateNatDiscovery(double now) {
  if (!m_stun) {
    return;
  }
  if (m_resolve) {
    std::vector<Address> servers;
    bool ready = false;
    {
      std::lock_guard<std::mutex> lock(m_resolve->mutex);
      if (m_resolve->done) {
        servers = std::move(m_resolve->servers);
        ready = true;
      }
    }
    if (ready) {
      m_resolve.reset();
      m_stun->discovery().start(servers);
    }
  }
  m_stun->update(now);

  if (m_discoveryPending && !m_resolve) {
    NatDiscovery::State state = m_stun->discovery().state();
    if (state == NatDiscovery::State::Done ||
        state == NatDiscovery::State::Failed) {
      m_discoveryPending = false;
      bool ok = state == NatDiscovery::State::Done;
      NatInfo info = m_stun->discovery().info();
      for (size_t i = 0; i < m_onNatDiscovery.size(); ++i) {
        m_onNatDiscovery[i](ok, info);
      }
    }
  }
}

NatDiscovery::State Network::natDiscoveryState() const {
  return m_stun ? m_stun->discovery().state() : NatDiscovery::State::Idle;
}

Address Network::publicAddress() const {
  return m_stun ? m_stun->discovery().info().publicAddress : Address();
}

NatType Network::natType() const {
  return m_stun ? m_stun->discovery().info().type : NatType::Unknown;
}

void Network::onNatDiscovery(
    std::function<void(bool, const NatInfo &)> handler) {
  m_onNatDiscovery.push_back(std::move(handler));
}

std::string Network::inviteCode() const {
  return encodeInvite(publicAddress());
}

bool Network::connectToInvite(const std::string &code) {
  Address peer;
  return decodeInvite(code, peer) && connect(peer);
}

void Network::punch(const Address &peerPublicAddress, double duration) {
  if (m_transport) {
    m_transport->punch(peerPublicAddress, duration);
  }
}

} // namespace componeng::net
