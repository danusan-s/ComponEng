#pragma once

#include <bitset>
#include <cstdint>

namespace componeng::ecs {

using EntityID = std::uint32_t;

constexpr EntityID INVALID_ENTITY = 0;

constexpr EntityID MAX_ENTITIES = 10000;

using ComponentID = std::uint8_t;

using ArchetypeID = std::uint8_t;

constexpr ComponentID MAX_COMPONENTS = 32;

constexpr ArchetypeID MAX_ARCHETYPES = 64;

// An entity with no components belongs to no archetype.
constexpr ArchetypeID INVALID_ARCHETYPE = MAX_ARCHETYPES;
static_assert(MAX_ARCHETYPES < UINT8_MAX,
              "INVALID_ARCHETYPE must not collide with a valid id");

using Signature = std::bitset<MAX_COMPONENTS>;

} // namespace componeng::ecs
