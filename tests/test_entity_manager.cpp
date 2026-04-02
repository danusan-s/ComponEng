#include <gtest/gtest.h>
#include "ecs/entity_manager.hpp"

TEST(EntityManagerTest, CreatesSequentialIDs) {
  EntityManager em;
  EXPECT_EQ(em.createEntity(), 0u);
  EXPECT_EQ(em.createEntity(), 1u);
  EXPECT_EQ(em.createEntity(), 2u);
}

TEST(EntityManagerTest, RecyclesDestroyedIDs) {
  EntityManager em;
  // Exhaust all pre-initialized IDs first
  for (EntityID i = 0; i < MAX_ENTITIES; ++i) {
    em.createEntity();
  }
  // Now destroy and recreate
  em.destroyEntity(0u);
  EXPECT_EQ(em.createEntity(), 0u);
}

TEST(EntityManagerTest, RecycledIDsGoToBackOfQueue) {
  EntityManager em;
  EntityID a = em.createEntity(); // 0
  EntityID b = em.createEntity(); // 1

  em.destroyEntity(a);
  // Queue front still has 2, 3, ... so next create returns 2, not 0
  EXPECT_EQ(em.createEntity(), 2u);
  // After exhausting unused IDs, recycled 0 comes back
  for (EntityID i = 3; i < MAX_ENTITIES; ++i) {
    em.createEntity();
  }
  EXPECT_EQ(em.createEntity(), 0u);
}

TEST(EntityManagerTest, TracksLivingEntityCount) {
  EntityManager em;
  em.createEntity();
  em.createEntity();
  em.createEntity();
  em.destroyEntity(1u);

  auto& rec = em.getRecord(1u);
  EXPECT_EQ(rec.signature.count(), 0u);
  EXPECT_EQ(rec.row, 0u);
}

TEST(EntityManagerTest, GetRecordReturnsCorrectData) {
  EntityManager em;
  EntityID id = em.createEntity();
  EntityRecord& rec = em.getRecord(id);
  EXPECT_EQ(rec.row, 0u);
  EXPECT_EQ(rec.signature.count(), 0u);
}

TEST(EntityManagerTest, DestroyedEntityRecordIsCleared) {
  EntityManager em;
  EntityID id = em.createEntity();
  em.destroyEntity(id);
  EntityRecord& rec = em.getRecord(id);
  EXPECT_EQ(rec.row, 0u);
  EXPECT_TRUE(rec.signature.none());
}
