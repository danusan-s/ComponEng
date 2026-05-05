#include "componeng/ecs/entity_manager.hpp"
#include <gtest/gtest.h>

TEST(EntityManagerTest, CreatesSequentialIDs) {
  componeng::ecs::EntityManager em;
  EXPECT_EQ(em.createEntity(), 1u);
  EXPECT_EQ(em.createEntity(), 2u);
  EXPECT_EQ(em.createEntity(), 3u);
}

TEST(EntityManagerTest, RecyclesDestroyedIDs) {
  componeng::ecs::EntityManager em;
  // Exhaust all pre-initialized IDs first (starts from 1, so MAX_ENTITIES-1
  // total)
  for (componeng::ecs::EntityID i = 1; i < componeng::ecs::MAX_ENTITIES; ++i) {
    em.createEntity();
  }
  // Now destroy and recreate
  em.destroyEntity(1u);
  EXPECT_EQ(em.createEntity(), 1u);
}

TEST(EntityManagerTest, RecycledIDsGoToBackOfQueue) {
  componeng::ecs::EntityManager em;
  componeng::ecs::EntityID a = em.createEntity(); // 1
  componeng::ecs::EntityID b = em.createEntity(); // 2

  em.destroyEntity(a);
  // Queue front still has 3, 4, ... so next create returns 3, not 1
  EXPECT_EQ(em.createEntity(), 3u);
  // After exhausting unused IDs, recycled 1 comes back
  for (componeng::ecs::EntityID i = 4; i < componeng::ecs::MAX_ENTITIES; ++i) {
    em.createEntity();
  }
  EXPECT_EQ(em.createEntity(), 1u);
}

TEST(EntityManagerTest, TracksLivingEntityCount) {
  componeng::ecs::EntityManager em;
  em.createEntity(); // 1
  em.createEntity(); // 2
  em.createEntity(); // 3
  em.destroyEntity(2u);

  auto &rec = em.getRecord(2u);
  EXPECT_EQ(rec.signature.count(), 0u);
  EXPECT_EQ(rec.row, 0u);
}

TEST(EntityManagerTest, GetRecordReturnsCorrectData) {
  componeng::ecs::EntityManager em;
  componeng::ecs::EntityID id = em.createEntity(); // 1
  componeng::ecs::EntityRecord &rec = em.getRecord(id);
  EXPECT_EQ(rec.row, 0u);
  EXPECT_EQ(rec.signature.count(), 0u);
}

TEST(EntityManagerTest, DestroyedEntityRecordIsCleared) {
  componeng::ecs::EntityManager em;
  componeng::ecs::EntityID id = em.createEntity(); // 1
  em.destroyEntity(id);
  componeng::ecs::EntityRecord &rec = em.getRecord(id);
  EXPECT_EQ(rec.row, 0u);
  EXPECT_TRUE(rec.signature.none());
}
