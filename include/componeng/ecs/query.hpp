#pragma once

#include "componeng/ecs/archetype/archetype.hpp"
#include "componeng/ecs/component_registry.hpp"
#include "componeng/ecs/entity.hpp"
#include "componeng/ecs/thread_pool.hpp"

#include <algorithm>
#include <future>
#include <vector>

namespace componeng::ecs {

struct QueryDesc {
  Signature required;
  Signature excluded;
};

template <typename... Req> class QueryIterator {
private:
  std::vector<Archetype *> &m_archetypes;
  size_t m_archetypeIndex;
  size_t m_rowIndex;

  ComponentRegistry &m_registry;

public:
  QueryIterator(std::vector<Archetype *> &archetypes,
                ComponentRegistry &registry, size_t archetypeIndex = 0,
                size_t rowIndex = 0)
      : m_archetypes(archetypes), m_archetypeIndex(archetypeIndex),
        m_rowIndex(rowIndex), m_registry(registry) {
  }

  QueryIterator &operator++() {
    if (m_archetypeIndex >= m_archetypes.size())
      return *this;

    ++m_rowIndex;
    if (m_rowIndex >= m_archetypes[m_archetypeIndex]->getEntityCount()) {
      ++m_archetypeIndex;
      m_rowIndex = 0;
    }
    return *this;
  }

  QueryIterator operator++(int) {
    QueryIterator tmp = *this;
    ++(*this);
    return tmp;
  }

  bool operator==(const QueryIterator &other) const {
    return m_archetypeIndex == other.m_archetypeIndex &&
           m_rowIndex == other.m_rowIndex;
  }

  bool operator!=(const QueryIterator &other) const {
    return !(*this == other);
  }

  std::tuple<Req &...> operator*() {
    Archetype *archetype = m_archetypes[m_archetypeIndex];
    return std::forward_as_tuple(
        archetype->get<Req>(m_registry.getComponentID<Req>(), m_rowIndex)...);
  }
};

/**
 * @brief Typed query over entities matching a set of required component
 * types.
 *
 * Iterates all archetypes whose Signature contains every requested
 * component and invokes a user-provided callback for each matching
 * entity. Also supports exclude() to filter out entities with specific
 * components, and eachParallel() to distribute work across the engine's
 * ThreadPool.
 *
 * @tparam Req... Component types that an entity must have to match.
 */
template <typename... Req> class Query {
private:
  std::array<Archetype, MAX_ARCHETYPES> &m_archetypes;
  std::vector<Archetype *> m_matchingArchetypes;
  bool m_dirty = true;
  ComponentRegistry &m_registry;
  QueryDesc desc;

public:
  bool matches(const Archetype &archetype) {
    return (archetype.m_signature & desc.required) == desc.required &&
           (archetype.m_signature & desc.excluded).none();
  }

  template <typename Fn, size_t... I>
  void invokeCallback(Fn &fn, ComponentColumn **reqCols, size_t i,
                      std::index_sequence<I...>) {
    fn(reqCols[I]->template get<Req>(i)...);
  }

  template <typename Fn, size_t... I>
  void invokeCallbackWithEntity(Fn &fn, EntityID entity,
                                ComponentColumn **reqCols, size_t i,
                                std::index_sequence<I...>) {
    fn(entity, reqCols[I]->template get<Req>(i)...);
  }

  void updateMatchingArchetypes() {
    if (!m_dirty)
      return;

    m_matchingArchetypes.clear();
    for (Archetype &archetype : m_archetypes) {
      if (matches(archetype)) {
        m_matchingArchetypes.push_back(&archetype);
      }
    }
    m_dirty = false;
  }

  Query(std::array<Archetype, MAX_ARCHETYPES> &archetypes,
        ComponentRegistry &registry)
      : m_archetypes(archetypes), m_registry(registry) {
    desc.required = m_registry.makeSignature<Req...>();
  }

  template <typename... Excl> Query<Req...> &exclude() {
    desc.excluded = m_registry.makeSignature<Excl...>();
    return *this;
  }

  template <typename Fn> void each(Fn fn) {
    updateMatchingArchetypes();

    for (Archetype *archetype : m_matchingArchetypes) {

      ComponentColumn *reqCols[] = {
          &archetype->getColumn(m_registry.getComponentID<Req>())...};

      size_t n = archetype->getEntityCount();
      for (size_t i = 0; i < n; ++i)
        invokeCallback(fn, reqCols, i, std::index_sequence_for<Req...>{});
    }
  }

  template <typename Fn> void eachWithEntity(Fn fn) {
    updateMatchingArchetypes();

    for (Archetype *archetype : m_matchingArchetypes) {
      ComponentColumn *reqCols[] = {
          &archetype->getColumn(m_registry.getComponentID<Req>())...};

      size_t n = archetype->getEntityCount();
      for (size_t i = 0; i < n; ++i)
        invokeCallbackWithEntity(fn, archetype->getEntityForRow(i), reqCols, i,
                                 std::index_sequence_for<Req...>{});
    }
  }

  template <typename Fn> void eachParallel(ThreadPool &pool, Fn fn) {
    std::vector<std::future<void>> futures;
    updateMatchingArchetypes();

    for (size_t a = 0; a < m_matchingArchetypes.size(); ++a) {
      Archetype *archetype = m_matchingArchetypes[a];

      size_t n = archetype->getEntityCount();
      if (n == 0)
        continue;

      // If less than 50 running non parallel is faster as there is a
      // overhead when we create task and then the pool unlocks and runs
      // it. For small number of entities, that overhead is more than the
      // time it takes to just run the loop in the current thread.
      if (n < 50) {
        ComponentColumn *reqCols[] = {
            &archetype->getColumn(m_registry.getComponentID<Req>())...};
        for (size_t i = 0; i < n; ++i) {
          invokeCallback(fn, reqCols, i, std::index_sequence_for<Req...>{});
        }
        continue;
      }

      size_t numChunks = std::min(n, pool.threadCount());
      size_t chunkSize = std::max(size_t(1), n / numChunks);

      for (size_t c = 0; c < numChunks; ++c) {
        size_t start = c * chunkSize;
        size_t end = (c == numChunks - 1) ? n : start + chunkSize;

        futures.push_back(pool.submit([this, a, start, end, fn]() {
          Archetype *chunkArchetype = m_matchingArchetypes[a];
          ComponentColumn *reqCols[] = {
              &chunkArchetype->getColumn(m_registry.getComponentID<Req>())...};

          for (size_t i = start; i < end; ++i) {
            invokeCallback(fn, reqCols, i, std::index_sequence_for<Req...>{});
          }
        }));
      }
    }

    for (auto &f : futures)
      f.wait();
  }

  QueryIterator<Req...> begin() {
    updateMatchingArchetypes();
    return QueryIterator<Req...>(m_matchingArchetypes, m_registry);
  }

  QueryIterator<Req...> end() {
    updateMatchingArchetypes();
    return QueryIterator<Req...>(m_matchingArchetypes, m_registry,
                                 m_matchingArchetypes.size(), 0);
  }
};

} // namespace componeng::ecs
