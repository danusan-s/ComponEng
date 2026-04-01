#pragma once
#include <bitset>
#include <cstdint>

using EntityID = std::uint32_t;

// Sentinel value for invalid/empty entity references.
// Uses max uint32 so that ID 0 remains a valid entity.
constexpr EntityID INVALID_ENTITY = 0xFFFFFFFFu;

constexpr EntityID MAX_ENTITIES = 10000;

using ComponentID = std::uint8_t; // one byte upto 255 components

using ArchetypeID = std::uint8_t; // one byte upto 255 archetypes

constexpr ComponentID MAX_COMPONENTS = 32;

constexpr ArchetypeID MAX_ARCHETYPES = 64;

// set nth bit for presence of component of id n
using Signature = std::bitset<MAX_COMPONENTS>;
