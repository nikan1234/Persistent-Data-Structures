#include <Collections/PersistentArray.h>

#include <gtest/gtest.h>

using namespace Persistence;

TEST(PersistentArrayTests, TestDefaultConstructor) {
  const PersistentArray<int> test;
  ASSERT_TRUE(test.empty());
  ASSERT_EQ(test.size(), 0u);
}
