#pragma once

#include <memory>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace componeng::resources {

struct IResource {
  virtual ~IResource() = default;
};

template <typename T> struct ResourceHolder : IResource {
  T value;
};

class ResourceManager {
public:
  template <typename T> void set(T &&value) {
    auto holder = std::make_unique<ResourceHolder<T>>();
    holder->value = std::forward<T>(value);
    m_resources[std::type_index(typeid(T))] = std::move(holder);
  }

  template <typename T> T &get() {
    auto it = m_resources.find(std::type_index(typeid(T)));
    if (it == m_resources.end()) {
      throw std::runtime_error("Resource not found");
    }
    return static_cast<ResourceHolder<T> *>(it->second.get())->value;
  }

private:
  std::unordered_map<std::type_index, std::unique_ptr<IResource>> m_resources;
};

} // namespace componeng::resources
