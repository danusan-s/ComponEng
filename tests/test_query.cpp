#include "componeng/ecs/archetype.hpp"
#include "componeng/ecs/query.hpp"
#include <gtest/gtest.h>

struct Pos {
  float x, y, z;
};
struct Vel {
  float vx, vy, vz;
};
struct Mass {
  float value;
};

static componeng::ecs::ComponentRegistry makeRegistry() {
  componeng::ecs::ComponentRegistry reg;
  reg.registerComponent<Pos>();
  reg.registerComponent<Vel>();
  reg.registerComponent<Mass>();
  return reg;
}

static std::array<componeng::ecs::Archetype, componeng::ecs::MAX_ARCHETYPES>
makeArchetypes(componeng::ecs::ComponentRegistry &reg) {
  std::array<componeng::ecs::Archetype, componeng::ecs::MAX_ARCHETYPES> archs;

  // Archetype 0: Pos only
  componeng::ecs::Signature sig0;
  sig0.set(reg.getComponentID<Pos>());
  archs[0].init(sig0, &reg);
  archs[0].addEntity(0);
  archs[0].addEntity(1);

  // Archetype 1: Pos + Vel
  componeng::ecs::Signature sig1;
  sig1.set(reg.getComponentID<Pos>());
  sig1.set(reg.getComponentID<Vel>());
  archs[1].init(sig1, &reg);
  archs[1].addEntity(2);
  archs[1].addEntity(3);

  // Archetype 2: Pos + Vel + Mass
  componeng::ecs::Signature sig2;
  sig2.set(reg.getComponentID<Pos>());
  sig2.set(reg.getComponentID<Vel>());
  sig2.set(reg.getComponentID<Mass>());
  archs[2].init(sig2, &reg);
  archs[2].addEntity(4);

  return archs;
}

TEST(QueryTest, MatchesRequiredComponents) {
  componeng::ecs::ComponentRegistry reg = makeRegistry();
  auto archs = makeArchetypes(reg);

  componeng::ecs::Query<Pos> q(archs, reg);

  // Archetype 0 has Pos
  EXPECT_TRUE(q.matches(archs[0]));
  // Archetype 1 has Pos + Vel (superset of Pos)
  EXPECT_TRUE(q.matches(archs[1]));
  // Archetype 2 has Pos + Vel + Mass (superset of Pos)
  EXPECT_TRUE(q.matches(archs[2]));
}

TEST(QueryTest, MatchesMultipleRequiredComponents) {
  componeng::ecs::ComponentRegistry reg = makeRegistry();
  auto archs = makeArchetypes(reg);

  componeng::ecs::Query<Pos, Vel> q(archs, reg);

  // Archetype 0 only has Pos, missing Vel
  EXPECT_FALSE(q.matches(archs[0]));
  // Archetype 1 has both
  EXPECT_TRUE(q.matches(archs[1]));
  // Archetype 2 has both (plus Mass)
  EXPECT_TRUE(q.matches(archs[2]));
}

TEST(QueryTest, ExcludeFiltersCorrectly) {
  componeng::ecs::ComponentRegistry reg = makeRegistry();
  auto archs = makeArchetypes(reg);

  componeng::ecs::Query<Pos, Vel> q(archs, reg);
  q.exclude<Mass>();

  // Archetype 2 has Mass, should be excluded
  EXPECT_FALSE(q.matches(archs[2]));
  // Archetype 1 has Pos + Vel but no Mass
  EXPECT_TRUE(q.matches(archs[1]));
}

TEST(QueryTest, EachIteratesMatchingArchetypes) {
  componeng::ecs::ComponentRegistry reg = makeRegistry();
  auto archs = makeArchetypes(reg);

  int count = 0;
  componeng::ecs::Query<Pos> q(archs, reg);
  q.each([&](Pos &p) { ++count; });

  // Archetype 0: 2 entities, Archetype 1: 2 entities, Archetype 2: 1 entity
  // All have Pos, so 5 total
  EXPECT_EQ(count, 5);
}

TEST(QueryTest, EachWithEntityProvidesEntityID) {
  componeng::ecs::ComponentRegistry reg = makeRegistry();
  auto archs = makeArchetypes(reg);

  std::vector<componeng::ecs::EntityID> entities;
  componeng::ecs::Query<Pos> q(archs, reg);
  q.eachWithEntity([&](componeng::ecs::EntityID entity, Pos &p) {
    entities.push_back(entity);
  });

  EXPECT_EQ(entities.size(), 5u);
  EXPECT_EQ(entities[0], 0u);
  EXPECT_EQ(entities[1], 1u);
  EXPECT_EQ(entities[2], 2u);
}

TEST(QueryTest, ExcludeReturnsReference) {
  componeng::ecs::ComponentRegistry reg = makeRegistry();
  auto archs = makeArchetypes(reg);

  componeng::ecs::Query<Pos, Vel> q(archs, reg);
  componeng::ecs::Query<Pos, Vel> &ref = q.exclude<Mass>();

  // Should return the same object
  EXPECT_EQ(&ref, &q);
  // Exclusion should be applied
  EXPECT_FALSE(q.matches(archs[2]));
}

TEST(QueryTest, EachWithExclude) {
  componeng::ecs::ComponentRegistry reg = makeRegistry();
  auto archs = makeArchetypes(reg);

  int count = 0;
  componeng::ecs::Query<Pos, Vel> q(archs, reg);
  q.exclude<Mass>();
  q.each([&](Pos &p, Vel &v) { ++count; });

  // Only archetype 1 matches (Pos + Vel, no Mass)
  EXPECT_EQ(count, 2);
}
