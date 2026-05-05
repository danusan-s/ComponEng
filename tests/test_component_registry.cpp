#include "componeng/ecs/component_registry.hpp"
#include <gtest/gtest.h>

struct TestCompA {
  int value;
};
struct TestCompB {
  float x, y;
};
struct TestCompC {
  double data[4];
};

TEST(ComponentRegistryTest, RegistersUniqueIDs) {
  componeng::ecs::ComponentRegistry reg;
  componeng::ecs::ComponentID idA = reg.registerComponent<TestCompA>();
  componeng::ecs::ComponentID idB = reg.registerComponent<TestCompB>();
  componeng::ecs::ComponentID idC = reg.registerComponent<TestCompC>();

  EXPECT_NE(idA, idB);
  EXPECT_NE(idB, idC);
  EXPECT_NE(idA, idC);
}

TEST(ComponentRegistryTest, ReturnsSameIDOnDuplicateRegister) {
  componeng::ecs::ComponentRegistry reg;
  componeng::ecs::ComponentID id1 = reg.registerComponent<TestCompA>();
  componeng::ecs::ComponentID id2 = reg.registerComponent<TestCompA>();
  EXPECT_EQ(id1, id2);
}

TEST(ComponentRegistryTest, GetComponentIDMatchesRegister) {
  componeng::ecs::ComponentRegistry reg;
  componeng::ecs::ComponentID regID = reg.registerComponent<TestCompA>();
  componeng::ecs::ComponentID getID = reg.getComponentID<TestCompA>();
  EXPECT_EQ(regID, getID);
}

TEST(ComponentRegistryTest, GetComponentCount) {
  componeng::ecs::ComponentRegistry reg;
  EXPECT_EQ(reg.getComponentCount(), 0u);
  reg.registerComponent<TestCompA>();
  EXPECT_EQ(reg.getComponentCount(), 1u);
  reg.registerComponent<TestCompB>();
  EXPECT_EQ(reg.getComponentCount(), 2u);
}

TEST(ComponentRegistryTest, GetComponentInfo) {
  componeng::ecs::ComponentRegistry reg;
  componeng::ecs::ComponentID id = reg.registerComponent<TestCompA>();
  componeng::ecs::ComponentInfo &info = reg.getComponentInfo(id);
  EXPECT_EQ(info.size, sizeof(TestCompA));
}

TEST(ComponentRegistryTest, MakeSignature) {
  componeng::ecs::ComponentRegistry reg;
  reg.registerComponent<TestCompA>();
  reg.registerComponent<TestCompB>();

  componeng::ecs::Signature sig = reg.makeSignature<TestCompA, TestCompB>();
  EXPECT_TRUE(sig.test(reg.getComponentID<TestCompA>()));
  EXPECT_TRUE(sig.test(reg.getComponentID<TestCompB>()));
  EXPECT_EQ(sig.count(), 2u);
}

TEST(ComponentRegistryTest, ThrowsOnUnregisteredType) {
  componeng::ecs::ComponentRegistry reg;
  EXPECT_THROW(reg.getComponentID<TestCompA>(), std::runtime_error);
}

TEST(ComponentRegistryTest, StoresDestructorForNonTrivialTypes) {
  struct NonTrivial {
    NonTrivial() : ptr(new int(42)) {
    }
    ~NonTrivial() {
      delete ptr;
    }
    int *ptr;
  };

  componeng::ecs::ComponentRegistry reg;
  componeng::ecs::ComponentID id = reg.registerComponent<NonTrivial>();
  componeng::ecs::ComponentInfo &info = reg.getComponentInfo(id);
  EXPECT_NE(info.destructor, nullptr);
}

TEST(ComponentRegistryTest, NoDestructorForTrivialTypes) {
  componeng::ecs::ComponentRegistry reg;
  componeng::ecs::ComponentID id = reg.registerComponent<TestCompA>();
  componeng::ecs::ComponentInfo &info = reg.getComponentInfo(id);
  EXPECT_EQ(info.destructor, nullptr);
}
