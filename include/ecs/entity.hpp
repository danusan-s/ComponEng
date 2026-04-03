#pragma once
#include <bitset>
#include <cstdint>

using EntityID = std::uint32_t;

constexpr EntityID INVALID_ENTITY = 0xFFFFFFFFu;

constexpr EntityID MAX_ENTITIES = 10000;

using ComponentID = std::uint8_t;

using ArchetypeID = std::uint8_t;

constexpr ComponentID MAX_COMPONENTS = 32;

constexpr ArchetypeID MAX_ARCHETYPES = 64;

using Signature = std::bitset<MAX_COMPONENTS>;
