#include <gtest/gtest.h>
#include "componeng/ecs/component_registry.hpp"

struct TestCompA { int value; };
struct TestCompB { float x, y; };
struct TestCompC { double data[4]; };

TEST(ComponentRegistryTest, RegistersUniqueIDs) {
  ComponentRegistry reg;
  ComponentID idA = reg.registerComponent<TestCompA>();
  ComponentID idB = reg.registerComponent<TestCompB>();
  ComponentID idC = reg.registerComponent<TestCompC>();

  EXPECT_NE(idA, idB);
  EXPECT_NE(idB, idC);
  EXPECT_NE(idA, idC);
}

TEST(ComponentRegistryTest, ReturnsSameIDOnDuplicateRegister) {
  ComponentRegistry reg;
  ComponentID id1 = reg.registerComponent<TestCompA>();
  ComponentID id2 = reg.registerComponent<TestCompA>();
  EXPECT_EQ(id1, id2);
}

TEST(ComponentRegistryTest, GetComponentIDMatchesRegister) {
  ComponentRegistry reg;
  ComponentID regID = reg.registerComponent<TestCompA>();
  ComponentID getID = reg.getComponentID<TestCompA>();
  EXPECT_EQ(regID, getID);
}

TEST(ComponentRegistryTest, GetComponentCount) {
  ComponentRegistry reg;
  EXPECT_EQ(reg.getComponentCount(), 0u);
  reg.registerComponent<TestCompA>();
  EXPECT_EQ(reg.getComponentCount(), 1u);
  reg.registerComponent<TestCompB>();
  EXPECT_EQ(reg.getComponentCount(), 2u);
}

TEST(ComponentRegistryTest, GetComponentInfo) {
  ComponentRegistry reg;
  ComponentID id = reg.registerComponent<TestCompA>();
  ComponentInfo& info = reg.getComponentInfo(id);
  EXPECT_EQ(info.size, sizeof(TestCompA));
}

TEST(ComponentRegistryTest, MakeSignature) {
  ComponentRegistry reg;
  reg.registerComponent<TestCompA>();
  reg.registerComponent<TestCompB>();

  Signature sig = reg.makeSignature<TestCompA, TestCompB>();
  EXPECT_TRUE(sig.test(reg.getComponentID<TestCompA>()));
  EXPECT_TRUE(sig.test(reg.getComponentID<TestCompB>()));
  EXPECT_EQ(sig.count(), 2u);
}

TEST(ComponentRegistryTest, ThrowsOnUnregisteredType) {
  ComponentRegistry reg;
  EXPECT_THROW(reg.getComponentID<TestCompA>(), std::runtime_error);
}

TEST(ComponentRegistryTest, StoresDestructorForNonTrivialTypes) {
  struct NonTrivial {
    NonTrivial() : ptr(new int(42)) {}
    ~NonTrivial() { delete ptr; }
    int* ptr;
  };

  ComponentRegistry reg;
  ComponentID id = reg.registerComponent<NonTrivial>();
  ComponentInfo& info = reg.getComponentInfo(id);
  EXPECT_NE(info.destructor, nullptr);
}

TEST(ComponentRegistryTest, NoDestructorForTrivialTypes) {
  ComponentRegistry reg;
  ComponentID id = reg.registerComponent<TestCompA>();
  ComponentInfo& info = reg.getComponentInfo(id);
  EXPECT_EQ(info.destructor, nullptr);
}
