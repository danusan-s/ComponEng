#pragma once

#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace componeng::ecs {

class IEventQueue {
public:
  virtual ~IEventQueue() = default;
};

template <typename EventType> class EventQueue : public IEventQueue {
public:
  void emit(const EventType &event) {
    m_events.push_back(event);
  }

  const std::vector<EventType> &getEvents() const {
    return m_events;
  }

private:
  std::vector<EventType> m_events;
};

class EventBus {
public:
  template <typename EventType> void emit(const EventType &event) {
    getQueue<EventType>().emit(event);
  }

  template <typename EventType> const std::vector<EventType> &getEvents() {
    return getQueue<EventType>().getEvents();
  }

private:
  std::unordered_map<std::type_index, std::unique_ptr<IEventQueue>> m_queues;

  template <typename EventType> EventQueue<EventType> &getQueue() {
    std::type_index typeIndex(typeid(EventType));
    auto it = m_queues.find(typeIndex);
    if (it == m_queues.end()) {
      auto queue = std::make_unique<EventQueue<EventType>>();
      it = m_queues.emplace(typeIndex, std::move(queue)).first;
    }
    return *static_cast<EventQueue<EventType> *>(it->second.get());
  }
};

} // namespace componeng::ecs
