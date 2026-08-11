#pragma once

#include "componeng/ecs/system.hpp"
#include "componeng/utils/logger.hpp"

#include <array>
#include <assert.h>
#include <memory>
#include <queue>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace componeng::ecs {

enum class SystemGroup { Initialization, Simulation, Presentation };

constexpr SystemGroup GROUP_ORDER[] = {SystemGroup::Initialization,
                                       SystemGroup::Simulation,
                                       SystemGroup::Presentation};
constexpr size_t NUM_GROUPS = sizeof(GROUP_ORDER) / sizeof(SystemGroup);

using SystemID = size_t;

/**
 * @brief Lightweight record pairing a system's type name with its shared
 * pointer.
 */
struct SystemRecord {
  const char *typeName;
  std::shared_ptr<ISystem> system;
};

class SystemGroupManager {
private:
  std::unordered_map<std::type_index, SystemID> m_typeToIndex;
  std::vector<std::vector<SystemID>> m_dependencies;
  std::vector<SystemRecord> m_systems;
  bool m_executionOrderBuilt = false;

public:
  template <typename T> std::shared_ptr<T> addSystem() {
    std::type_index typeIndex = typeid(T);
    if (m_typeToIndex.find(typeIndex) != m_typeToIndex.end()) {
      throw std::runtime_error("System already registered in this group.");
    }
    auto system = std::make_shared<T>();
    m_systems.push_back({typeIndex.name(), system});
    m_dependencies.push_back({});
    m_typeToIndex[typeIndex] = m_systems.size() - 1;
    return system;
  }

  template <typename T, typename... Ts> void addSystemDependencies() {
    SystemID systemID = m_typeToIndex[typeid(T)];
    std::vector<SystemID> deps;
    (
        [&]() {
          if (!isRegistered<Ts>()) {
            addSystem<Ts>();
          }
          deps.push_back(m_typeToIndex[typeid(Ts)]);
        }(),
        ...);
    m_dependencies[systemID] = deps;
  }

  template <typename T> bool isRegistered() const {
    std::type_index typeIndex = typeid(T);
    return m_typeToIndex.find(typeIndex) != m_typeToIndex.end();
  }

  void buildExecutionOrder() {
    std::vector<size_t> inDegree(m_systems.size(), 0);
    std::vector<std::vector<SystemID>> reverseGraph(m_systems.size());

    for (int i = 0; i < m_dependencies.size(); ++i) {
      inDegree[i] = m_dependencies[i].size();
      for (SystemID dep : m_dependencies[i]) {
        reverseGraph[dep].push_back(i);
      }
    }

    std::queue<SystemID> q;

    for (int i = 0; i < inDegree.size(); ++i) {
      if (inDegree[i] == 0) {
        q.push(i);
      }
    }

    std::vector<SystemRecord> sortedSystems;
    while (!q.empty()) {
      SystemID current = q.front();
      q.pop();
      sortedSystems.push_back(m_systems[current]);
      for (SystemID dependent : reverseGraph[current]) {
        inDegree[dependent]--;
        if (inDegree[dependent] == 0) {
          q.push(dependent);
        }
      }
    }
    m_executionOrderBuilt = true;
    m_systems = std::move(sortedSystems);
  }

  void createAll(SystemState &state) {
    if (!m_executionOrderBuilt) {
      buildExecutionOrder();
    }
    for (auto const &sysRec : m_systems) {
      LOG_INFO("Creating system: %s", sysRec.typeName);
      auto const &system = sysRec.system;
      system->onCreate(state);
    }
  }

  void updateAll(SystemState &state) {
    if (!m_executionOrderBuilt) {
      buildExecutionOrder();
    }
    for (auto const &sysRec : m_systems) {
      auto const &system = sysRec.system;
      system->onUpdate(state);
    }
  }

  void destroyAll(SystemState &state) {
    if (!m_executionOrderBuilt) {
      buildExecutionOrder();
    }
    for (auto const &sysRec : m_systems) {
      LOG_INFO("Destroying system: %s", sysRec.typeName);
      auto const &system = sysRec.system;
      system->onDestroy(state);
    }
  }
};

/**
 * @brief Scheduler that registers, groups, and executes ECS systems.
 *
 * Systems are organized into three fixed groups (Initialization, Simulation,
 * Presentation) and run sequentially each frame in that order.
 * Duplicate registration of the same system type is prevented at runtime.
 */
class SystemManager {
private:
  std::array<SystemGroupManager, NUM_GROUPS> m_systemGroups;

public:
  template <typename T>
  std::shared_ptr<T> registerSystem(SystemGroup groupType) {
    size_t groupIndex = static_cast<size_t>(groupType);
    if (groupIndex >= NUM_GROUPS) {
      throw std::runtime_error("Invalid SystemGroupType.");
    }
    return m_systemGroups[groupIndex].addSystem<T>();
  }

  template <typename T, typename... Ts> void addSystemDependencies() {
    for (SystemGroupManager &group : m_systemGroups) {
      if (group.isRegistered<T>()) {
        group.addSystemDependencies<T, Ts...>();
        return;
      }
    }
  }

  void createAll(World *world) {
    SystemState state{world, 0.0f};
    for (SystemGroupManager &group : m_systemGroups) {
      group.buildExecutionOrder();
      group.createAll(state);
    }
  }

  void updateAll(World *world, float deltaTime) {
    SystemState state{world, deltaTime};
    for (SystemGroupManager &group : m_systemGroups) {
      group.updateAll(state);
    }
  }

  void destroyAll(World *world) {
    SystemState state{world, 0.0f};
    for (SystemGroupManager &group : m_systemGroups) {
      group.destroyAll(state);
    }
  }
};

} // namespace componeng::ecs
