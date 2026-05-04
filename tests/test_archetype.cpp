#include <gtest/gtest.h>
#include "componeng/ecs/archetype.hpp"

struct Pos { float x, y, z; };
struct Vel { float vx, vy, vz; };

static ComponentRegistry makeRegistry() {
  ComponentRegistry reg;
  reg.registerComponent<Pos>();
  reg.registerComponent<Vel>();
  return reg;
}

TEST(ArchetypeTest, InitSetsSignature) {
  ComponentRegistry reg = makeRegistry();
  Signature sig;
  sig.set(reg.getComponentID<Pos>());

  Archetype arch;
  arch.init(sig, &reg);
  EXPECT_EQ(arch.m_signature, sig);
}

TEST(ArchetypeTest, AddEntityIncreasesCount) {
  ComponentRegistry reg = makeRegistry();
  Signature sig;
  sig.set(reg.getComponentID<Pos>());

  Archetype arch;
  arch.init(sig, &reg);
  arch.addEntity(0);
  arch.addEntity(1);

  EXPECT_EQ(arch.getEntityCount(), 2u);
}

TEST(ArchetypeTest, GetColumnReturnsValidColumn) {
  ComponentRegistry reg = makeRegistry();
  Signature sig;
  sig.set(reg.getComponentID<Pos>());

  Archetype arch;
  arch.init(sig, &reg);
  arch.addEntity(0);

  auto& col = arch.getColumn(reg.getComponentID<Pos>());
  EXPECT_EQ(col.m_count, 1u);
  EXPECT_EQ(col.m_stride, sizeof(Pos));
}

TEST(ArchetypeTest, GetReturnsCorrectComponent) {
  ComponentRegistry reg = makeRegistry();
  Signature sig;
  sig.set(reg.getComponentID<Pos>());

  Archetype arch;
  arch.init(sig, &reg);
  arch.addEntity(0);

  Pos& p = arch.get<Pos>(reg.getComponentID<Pos>(), 0);
  p.x = 1.0f;
  p.y = 2.0f;
  p.z = 3.0f;

  Pos& p2 = arch.get<Pos>(reg.getComponentID<Pos>(), 0);
  EXPECT_FLOAT_EQ(p2.x, 1.0f);
  EXPECT_FLOAT_EQ(p2.y, 2.0f);
  EXPECT_FLOAT_EQ(p2.z, 3.0f);
}

TEST(ArchetypeTest, RemoveEntityDecreasesCount) {
  ComponentRegistry reg = makeRegistry();
  Signature sig;
  sig.set(reg.getComponentID<Pos>());

  Archetype arch;
  arch.init(sig, &reg);
  arch.addEntity(0);
  arch.addEntity(1);
  arch.addEntity(2);

  arch.removeEntity(1);
  EXPECT_EQ(arch.getEntityCount(), 2u);
}

TEST(ArchetypeTest, RemoveEntityReturnsSwappedEntity) {
  ComponentRegistry reg = makeRegistry();
  Signature sig;
  sig.set(reg.getComponentID<Pos>());

  Archetype arch;
  arch.init(sig, &reg);
  arch.addEntity(10);
  arch.addEntity(20);

  // Removing entity 10 should swap entity 20 into its slot
  EntityID swapped = arch.removeEntity(10);
  EXPECT_EQ(swapped, 20u);
  EXPECT_EQ(arch.getEntityForRow(0), 20u);
}

TEST(ArchetypeTest, GetRowForEntity) {
  ComponentRegistry reg = makeRegistry();
  Signature sig;
  sig.set(reg.getComponentID<Pos>());

  Archetype arch;
  arch.init(sig, &reg);
  arch.addEntity(5);
  arch.addEntity(10);

  EXPECT_EQ(arch.getRowForEntity(5), 0u);
  EXPECT_EQ(arch.getRowForEntity(10), 1u);
}

TEST(ArchetypeTest, ThrowsOnInvalidColumn) {
  ComponentRegistry reg = makeRegistry();
  Signature sig;
  sig.set(reg.getComponentID<Pos>());

  Archetype arch;
  arch.init(sig, &reg);
  arch.addEntity(0);

  EXPECT_THROW(arch.getColumn(reg.getComponentID<Vel>()), std::runtime_error);
}

TEST(ArchetypeTest, MultiComponentArchetype) {
  ComponentRegistry reg = makeRegistry();
  Signature sig;
  sig.set(reg.getComponentID<Pos>());
  sig.set(reg.getComponentID<Vel>());

  Archetype arch;
  arch.init(sig, &reg);
  arch.addEntity(0);

  Pos& p = arch.get<Pos>(reg.getComponentID<Pos>(), 0);
  p.x = 5.0f;

  Vel& v = arch.get<Vel>(reg.getComponentID<Vel>(), 0);
  v.vx = 1.0f;

  EXPECT_FLOAT_EQ(arch.get<Pos>(reg.getComponentID<Pos>(), 0).x, 5.0f);
  EXPECT_FLOAT_EQ(arch.get<Vel>(reg.getComponentID<Vel>(), 0).vx, 1.0f);
}
