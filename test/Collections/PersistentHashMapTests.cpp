#include <Collections/PersistentHashMap.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace testing;
using namespace Persistence;

TEST(PersistentHashMapTests, TestDefaultConstructor) {
  const PersistentHashMap<std::string, int> empty;
  EXPECT_TRUE(empty.empty());
  EXPECT_EQ(empty.size(), 0);
}

TEST(PersistentHashMapTests, TestInitializerListConstructor) {
  const PersistentHashMap<std::string, int> test{{"x", 1}, {"y", 2}, {"z", 3}, {"x", 4}};
  using ValueType = decltype(test)::value_type;

  EXPECT_FALSE(test.empty());
  EXPECT_EQ(test.size(), 3);
  EXPECT_THAT(test, UnorderedElementsAre(ValueType{"x", 1}, ValueType{"y", 2}, ValueType{"z", 3}));
}

TEST(PersistentHashMapTests, TestInsert) {
  const PersistentHashMap<std::string, int> v0;
  using ValueType = decltype(v0)::value_type;

  const auto v1 = v0.insert({"a", 1});
  const auto v2 = v1.insert({"b", 2});
  const auto v3 = v2.insert({"b", 3});
  const auto v4 = v2.insert({"b", 4}, false);

  EXPECT_EQ(v0.size(), 0);
  EXPECT_EQ(v1.size(), 1);
  EXPECT_EQ(v2.size(), 2);
  EXPECT_EQ(v3.size(), 2);
  EXPECT_EQ(v4.size(), 2);

  EXPECT_THAT(v0, UnorderedElementsAre());
  EXPECT_THAT(v1, UnorderedElementsAre(ValueType{"a", 1}));
  EXPECT_THAT(v2, UnorderedElementsAre(ValueType{"a", 1}, ValueType{"b", 2}));
  EXPECT_THAT(v3, UnorderedElementsAre(ValueType{"a", 1}, ValueType{"b", 3}));
  EXPECT_THAT(v4, UnorderedElementsAre(ValueType{"a", 1}, ValueType{"b", 2}));
}

TEST(PersistentHashMapTests, TestErase) {
  const PersistentHashMap<std::string, int> empty;
  using ValueType = decltype(empty)::value_type;

  const auto v0 = empty.insert({"x", 10}).insert({"y", 20}).insert({"z", 30});
  const auto v1 = v0.erase("x");
  const auto v2 = v1.erase("y");
  const auto v3 = v2.erase("z");
  const auto v4 = v2.erase("not existing");
  const auto v5 = v3.erase("not existing");

  EXPECT_EQ(v0.size(), 3);
  EXPECT_EQ(v1.size(), 2);
  EXPECT_EQ(v2.size(), 1);
  EXPECT_EQ(v3.size(), 0);
  EXPECT_EQ(v4.size(), 1);
  EXPECT_EQ(v5.size(), 0);

  EXPECT_THAT(v0, UnorderedElementsAre(ValueType{"x", 10}, ValueType{"y", 20}, ValueType{"z", 30}));
  EXPECT_THAT(v1, UnorderedElementsAre(ValueType{"y", 20}, ValueType{"z", 30}));
  EXPECT_THAT(v2, UnorderedElementsAre(ValueType{"z", 30}));
  EXPECT_THAT(v3, UnorderedElementsAre());
  EXPECT_THAT(v4, UnorderedElementsAre(ValueType{"z", 30}));
  EXPECT_THAT(v5, UnorderedElementsAre());
}

TEST(PersistentHashMapTests, UndoRedo) {
  const PersistentHashMap<std::string, int> v0{{"x", 1}, {"y", 2}};
  using ValueType = decltype(v0)::value_type;

  const auto v1 = v0.insert({"z", 3});
  const auto v2 = v0.insert({"x", 4});
  const auto v3 = v0.insert({"x", 5}, false);
  const auto v4 = v1.undo().redo();
  const auto v5 = v2.undo().redo();
  const auto v6 = v3.undo().redo();

  EXPECT_EQ(v0.size(), 2);
  EXPECT_EQ(v1.size(), 3);
  EXPECT_EQ(v2.size(), 2);
  EXPECT_EQ(v3.size(), 2);

  EXPECT_EQ(v1.undo().size(), 2);
  EXPECT_EQ(v2.undo().size(), 2);
  EXPECT_EQ(v3.undo().size(), 2);

  EXPECT_EQ(v4.size(), 3);
  EXPECT_EQ(v5.size(), 2);
  EXPECT_EQ(v6.size(), 2);

  EXPECT_THAT(v0, UnorderedElementsAre(ValueType{"x", 1}, ValueType{"y", 2}));
  EXPECT_THAT(v1, UnorderedElementsAre(ValueType{"x", 1}, ValueType{"y", 2}, ValueType{"z", 3}));
  EXPECT_THAT(v2, UnorderedElementsAre(ValueType{"x", 4}, ValueType{"y", 2}));
  EXPECT_THAT(v3, UnorderedElementsAre(ValueType{"x", 1}, ValueType{"y", 2}));

  EXPECT_THAT(v1.undo(), UnorderedElementsAre(ValueType{"x", 1}, ValueType{"y", 2}));
  EXPECT_THAT(v2.undo(), UnorderedElementsAre(ValueType{"x", 1}, ValueType{"y", 2}));
  EXPECT_THAT(v3.undo(), UnorderedElementsAre(ValueType{"x", 1}, ValueType{"y", 2}));

  EXPECT_THAT(v4, UnorderedElementsAre(ValueType{"x", 1}, ValueType{"y", 2}, ValueType{"z", 3}));
  EXPECT_THAT(v5, UnorderedElementsAre(ValueType{"x", 4}, ValueType{"y", 2}));
  EXPECT_THAT(v6, UnorderedElementsAre(ValueType{"x", 1}, ValueType{"y", 2}));
}
