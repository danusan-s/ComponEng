#pragma once
#include <bitset>
#include <cstdint>

using EntityID = std::uint32_t;

constexpr EntityID INVALID_ENTITY = 0;

constexpr EntityID MAX_ENTITIES = 10000;

using ComponentID = std::uint8_t; // one byte upto 255 components

using ArchetypeID = std::uint8_t; // one byte upto 255 archetypes

constexpr ComponentID MAX_COMPONENTS = 32;

constexpr ArchetypeID MAX_ARCHETYPES = 64;

// set nth bit for presence of component of id n
using Signature = std::bitset<MAX_COMPONENTS>;
