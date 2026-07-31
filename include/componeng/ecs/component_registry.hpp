#pragma once

#include "componeng/components/component_serializer.hpp"
#include "entity.hpp"
#include <array>
#include <stdexcept>
#include <string_view>
#include <typeindex>
#include <unordered_map>

namespace componeng::ecs {

/**
 * @brief Runtime metadata associated with a registered component type.
 *
 * Stores the type name, size in bytes, and an optional destructor
 * callback for non-trivially-destructible types.
 */
struct ComponentInfo {
  const char *name;
  size_t size;
  size_t alignment;
  void (*destructor)(void *) = nullptr;
  nlohmann::json (*serializer)(const void *) = nullptr;
  void *(*deserializer)(const nlohmann::json &) = nullptr;
};

/**
 * @brief Maps types to compact ComponentIDs at runtime.
 *
 * Each distinct component type is assigned a unique ID the first time
 * it is registered. These IDs are used as bit indices in Signatures
 * and as column indices within archetypes.
 */
class ComponentRegistry {
private:
  std::unordered_map<std::type_index, ComponentID> m_typeToID;
  std::unordered_map<std::string_view, ComponentID> m_nameToID;
  std::array<ComponentInfo, MAX_COMPONENTS> m_componentInfos;
  ComponentID m_nextComponentID = 0;

public:
  template <typename T> ComponentID registerComponent() {
    static_assert(!std::is_pointer_v<T>,
                  "Component type must not be a pointer");
    static_assert(!std::is_reference_v<T>,
                  "Component type must not be a reference");
    static_assert(std::is_move_constructible_v<T>,
                  "Component must be move-constructible");

    constexpr const char *typeName = T::component_name;
    static_assert(
        typeName != nullptr,
        "Component type is missing a component_name. "
        "Add 'static constexpr const char* component_name = \"...\";' "
        "to the component struct.");

    const std::type_index key = typeid(T);
    auto it = m_typeToID.find(key);
    if (it != m_typeToID.end())
      return it->second;

    if (m_nextComponentID >= MAX_COMPONENTS)
      throw std::runtime_error("ComponentRegistry: MAX_COMPONENTS exceeded");

    auto nameIt = m_nameToID.find(typeName);
    if (nameIt != m_nameToID.end()) {
      throw std::runtime_error(
          std::string("Duplicate component name '") + typeName +
          "' — two different component types cannot share the same name");
    }

    ComponentID id = m_nextComponentID++;
    m_typeToID[key] = id;
    m_componentInfos[id].name = typeName;
    m_nameToID[typeName] = id;
    m_componentInfos[id].size = sizeof(T);
    m_componentInfos[id].alignment = alignof(T);
    // Store destructor for non-trivial types so archetype moves can clean up.
    if constexpr (!std::is_trivially_destructible_v<T>) {
      m_componentInfos[id].destructor = [](void *ptr) {
        static_cast<T *>(ptr)->~T();
      };
    }
    // Store serializer and deserializer if specialization exists for this type.
    if constexpr (std::is_base_of_v<components::ComponentSerializer<T>,
                                    components::ComponentSerializer<T>>) {
      m_componentInfos[id].serializer =
          [](const void *componentPtr) -> nlohmann::json {
        const T &component = *static_cast<const T *>(componentPtr);
        return components::ComponentSerializer<T>::serialize(component);
      };
      m_componentInfos[id].deserializer =
          [](const nlohmann::json &json) -> void * {
        T component = components::ComponentSerializer<T>::deserialize(json);
        return new T(std::move(component));
      };
    }

    return id;
  }

  ComponentID getComponentIDByName(const std::string_view &name) const {
    auto it = m_nameToID.find(name);
    if (it == m_nameToID.end())
      throw std::runtime_error("ComponentRegistry: unknown component name");
    return it->second;
  }

  template <typename T> ComponentID getComponentID() const {
    auto it = m_typeToID.find(typeid(T));
    if (it == m_typeToID.end())
      throw std::runtime_error(
          "ComponentRegistry: unregistered component type");
    return it->second;
  }

  ComponentID getComponentCount() const {
    return m_nextComponentID;
  }

  ComponentInfo &getComponentInfo(ComponentID id) {
    if (id >= m_nextComponentID)
      throw std::runtime_error("ComponentRegistry: invalid ComponentID");
    return m_componentInfos[id];
  }

  template <typename... Ts> Signature makeSignature() const {
    Signature sig;
    (sig.set(getComponentID<Ts>()), ...);
    return sig;
  }
};

} // namespace componeng::ecs
