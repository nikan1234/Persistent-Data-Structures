#include <Collections/PersistentArray.h>

#include "../TestUtils/OperatorsCountingElement.h"

#include <gtest/gtest.h>
#include <numeric>

using namespace Persistence;

/// Function to check array contents within iterators
namespace {
template <class T>
void checkElementsAre(const PersistentArray<T> &array, const std::initializer_list<T> &expected) {
  ASSERT_EQ(array.size(), expected.size());

  std::size_t index = 0;
  for (const auto &value : expected)
    ASSERT_EQ(array.value(index++), value);
}
} // namespace

TEST(PersistentArrayTests, TestDefaultConstructor) {
  const PersistentArray<int> test;
  EXPECT_TRUE(test.empty());
  EXPECT_EQ(test.size(), 0u);
}

TEST(PersistentArrayTests, TestInitalizerListConstrctor) {
  const PersistentArray<int> test{1, 2, 3, 4, 5};
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(test, {1, 2, 3, 4, 5}));
}

TEST(PersistentArrayTests, TestValueConstructor) {
  constexpr auto elementCount = 3;
  const PersistentArray<int> test(elementCount, 100);
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(test, {100, 100, 100}));
}

TEST(PersistentArrayTests, TestFrontBack) {
  const PersistentArray<int> test{1, 2, 3, 4, 5};
  EXPECT_EQ(test.front(), 1);
  EXPECT_EQ(test.back(), 5);
}

TEST(PersistentArrayTests, TestSetValue) {
  constexpr auto elementCount = 3;
  const PersistentArray<int> v0(elementCount, 100);

  const auto v1 = v0.setValue(0, 200);
  const auto v2 = v1.setValue(1, 300);
  const auto v3 = v0.setValue(2, 400);

  EXPECT_NO_FATAL_FAILURE(checkElementsAre(v0, {100, 100, 100}));
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(v1, {200, 100, 100}));
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(v2, {200, 300, 100}));
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(v3, {100, 100, 400}));
}

TEST(PersistentArrayTests, TestPushBack_1) {
  const PersistentArray<int> empty;

  const auto v1 = empty.pushBack(1);
  const auto v2 = v1.pushBack(2);
  const auto v3 = empty.pushBack(3);
  const auto v4 = v3.pushBack(4).pushBack(5).pushBack(6);

  EXPECT_NO_FATAL_FAILURE(checkElementsAre(v1, {1}));
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(v2, {1, 2}));
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(v3, {3}));
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(v4, {3, 4, 5, 6}));
}

TEST(PersistentArrayTests, TestPushBack_2) {
  constexpr auto elementCount = 3;

  PersistentArray<int> test;
  for (int i = 0; i < elementCount; ++i)
    test = test.pushBack(i);

  EXPECT_NO_FATAL_FAILURE(checkElementsAre(test, {0, 1, 2}));
}

TEST(PersistentArrayTests, TestPopBack_1) {
  const PersistentArray<int> v0{1, 2, 3};

  const auto v1 = v0.popBack();
  const auto v2 = v1.popBack().popBack();
  const auto v3 = v1.popBack();

  EXPECT_NO_FATAL_FAILURE(checkElementsAre(v0, {1, 2, 3}));
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(v1, {1, 2}));
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(v2, {}));
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(v3, {1}));
}

TEST(PersistentArrayTests, TestPopBack_2) {
  constexpr auto elementCount = 3;

  PersistentArray<int> test(3, 100);
  for (int i = 0; i < elementCount; ++i)
    test = test.popBack();

  EXPECT_TRUE(test.empty());
  EXPECT_EQ(test.size(), 0u);
}

TEST(PersistentArrayTests, TestUndoRedo_1) {
  const PersistentArray<int> v0{1, 2, 3};

  const auto v1 = v0.pushBack(100).setValue(2, 200);
  const auto v2 = v1.undo().undo();
  const auto v3 = v1.undo();
  const auto v4 = v2.redo();
  const auto v5 = v4.redo();
  const auto v6 = v2.redo().redo().pushBack(400);

  EXPECT_NO_FATAL_FAILURE(checkElementsAre(v0, {1, 2, 3}));
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(v1, {1, 2, 200, 100}));
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(v2, {1, 2, 3}));
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(v3, {1, 2, 3, 100}));
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(v4, {1, 2, 3, 100}));
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(v5, {1, 2, 200, 100}));
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(v6, {1, 2, 200, 100, 400}));
}

TEST(PersistentArrayTests, TestUndoRedo_2) {
  PersistentArray<int> test;

  test = test.pushBack(1);
  test = test.pushBack(2);
  test = test.pushBack(3);

  EXPECT_NO_FATAL_FAILURE(checkElementsAre(test, {1, 2, 3}));
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(test.undo(), {1, 2}));
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(test.undo().undo(), {1}));
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(test.undo().undo().undo(), {}));

  test = test.setValue(0, 4);
  test = test.setValue(1, 5);
  test = test.setValue(2, 6);

  EXPECT_NO_FATAL_FAILURE(checkElementsAre(test, {4, 5, 6}));
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(test.undo(), {4, 5, 3}));
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(test.undo().undo(), {4, 2, 3}));
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(test.undo().undo().undo(), {1, 2, 3}));

  test = test.popBack();
  test = test.popBack();
  test = test.popBack();

  EXPECT_NO_FATAL_FAILURE(checkElementsAre(test, {}));
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(test.undo(), {4}));
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(test.undo().undo(), {4, 5}));
  EXPECT_NO_FATAL_FAILURE(checkElementsAre(test.undo().undo().undo(), {4, 5, 6}));
}

TEST(PersistentArrayTests, TestNoLeaks) {
  // Default constructed
  { PersistentArray<UnitTests::OperatorsCountingElement> test; }
  EXPECT_EQ(UnitTests::OperatorsCountingElement::defaultConstructorCalls(), 0);
  EXPECT_EQ(UnitTests::OperatorsCountingElement::copyConstructorCalls(), 0);
  EXPECT_EQ(UnitTests::OperatorsCountingElement::moveConstructorCalls(), 0);
  EXPECT_EQ(UnitTests::OperatorsCountingElement::copyOperatorCalls(), 0);
  EXPECT_EQ(UnitTests::OperatorsCountingElement::moveOperatorCalls(), 0);
  EXPECT_EQ(UnitTests::OperatorsCountingElement::destructorCalls(), 0);

  // Value-based constructor
  {
    {
      constexpr auto elementsCount = 3;
      PersistentArray test(elementsCount, UnitTests::OperatorsCountingElement{});
      EXPECT_EQ(UnitTests::OperatorsCountingElement::defaultConstructorCalls(), 1);
      EXPECT_EQ(UnitTests::OperatorsCountingElement::copyConstructorCalls(), 3);
      EXPECT_EQ(UnitTests::OperatorsCountingElement::moveConstructorCalls(), 0);
      EXPECT_EQ(UnitTests::OperatorsCountingElement::copyOperatorCalls(), 0);
      EXPECT_EQ(UnitTests::OperatorsCountingElement::moveOperatorCalls(), 0);
      EXPECT_EQ(UnitTests::OperatorsCountingElement::destructorCalls(), 1);
      UnitTests::OperatorsCountingElement::reset();
    }
    EXPECT_EQ(UnitTests::OperatorsCountingElement::destructorCalls(), 3);
    UnitTests::OperatorsCountingElement::reset();
  }

  /// Push back
  {
    {
      PersistentArray test(3, UnitTests::OperatorsCountingElement{});
      test = test.pushBack(UnitTests::OperatorsCountingElement{});
      test = test.pushBack(UnitTests::OperatorsCountingElement{});
      UnitTests::OperatorsCountingElement::reset();

      test = test.popBack();
      EXPECT_EQ(UnitTests::OperatorsCountingElement::destructorCalls(), 0);
    }
    EXPECT_EQ(UnitTests::OperatorsCountingElement::destructorCalls(), 5);
    UnitTests::OperatorsCountingElement::reset();
  }

  /// Set value
  {
    {
      PersistentArray v0(3, UnitTests::OperatorsCountingElement{});
      UnitTests::OperatorsCountingElement::reset();

      const auto v1 = v0.setValue(0, UnitTests::OperatorsCountingElement{});
      const auto v2 = v1.setValue(1, UnitTests::OperatorsCountingElement{});
      EXPECT_EQ(UnitTests::OperatorsCountingElement::defaultConstructorCalls(), 2);
      EXPECT_EQ(UnitTests::OperatorsCountingElement::copyConstructorCalls(), 0);
      EXPECT_EQ(UnitTests::OperatorsCountingElement::moveConstructorCalls(), 2);
      EXPECT_EQ(UnitTests::OperatorsCountingElement::copyOperatorCalls(), 0);
      EXPECT_EQ(UnitTests::OperatorsCountingElement::moveOperatorCalls(), 0);
      EXPECT_EQ(UnitTests::OperatorsCountingElement::destructorCalls(), 2);
      UnitTests::OperatorsCountingElement::reset();
    }
    EXPECT_EQ(UnitTests::OperatorsCountingElement::destructorCalls(), 5);
    UnitTests::OperatorsCountingElement::reset();
  }

  /// Undo/redo
  {
    {
      PersistentArray<UnitTests::OperatorsCountingElement> v0;
      const auto v1 = v0.pushBack(UnitTests::OperatorsCountingElement{})
                          .pushBack(UnitTests::OperatorsCountingElement{});
      UnitTests::OperatorsCountingElement::reset();

      const auto v2 = v1.undo().redo();
      EXPECT_EQ(UnitTests::OperatorsCountingElement::defaultConstructorCalls(), 0);
      EXPECT_EQ(UnitTests::OperatorsCountingElement::copyConstructorCalls(), 0);
      EXPECT_EQ(UnitTests::OperatorsCountingElement::moveConstructorCalls(), 0);
      EXPECT_EQ(UnitTests::OperatorsCountingElement::copyOperatorCalls(), 0);
      EXPECT_EQ(UnitTests::OperatorsCountingElement::moveOperatorCalls(), 0);
      EXPECT_EQ(UnitTests::OperatorsCountingElement::destructorCalls(), 0);

      const auto v3 = v2.setValue(0, UnitTests::OperatorsCountingElement{});
      UnitTests::OperatorsCountingElement::reset();

      const auto v4 = v3.undo().popBack();
      EXPECT_EQ(UnitTests::OperatorsCountingElement::defaultConstructorCalls(), 0);
      EXPECT_EQ(UnitTests::OperatorsCountingElement::copyConstructorCalls(), 0);
      EXPECT_EQ(UnitTests::OperatorsCountingElement::moveConstructorCalls(), 0);
      EXPECT_EQ(UnitTests::OperatorsCountingElement::copyOperatorCalls(), 0);
      EXPECT_EQ(UnitTests::OperatorsCountingElement::moveOperatorCalls(), 0);
      EXPECT_EQ(UnitTests::OperatorsCountingElement::destructorCalls(), 0);
    }
    EXPECT_EQ(UnitTests::OperatorsCountingElement::destructorCalls(), 3);
    UnitTests::OperatorsCountingElement::reset();
  }
}

///iterator
TEST(PersistentArrayTests, TestIterator1) {
  PersistentArray<int> test{1, 2, 3, 4};
  auto test1 = test.pushBack(5);
  auto test2 = test.popBack();
  auto test3 = test2.undo();
  EXPECT_EQ(std::accumulate(test.begin(), test.end(), 0), 10);
  EXPECT_EQ(std::accumulate(test1.begin(), test1.end(), 0), 15);
  EXPECT_EQ(std::accumulate(test2.begin(), test2.end(), 0), 6);
  EXPECT_EQ(std::accumulate(test3.begin(), test3.end(), 0), 10);
  EXPECT_EQ(std::accumulate(test3.begin(), test3.end(), 0), 10);
}

TEST(PersistentArrayTests, TestIterator2) {
  PersistentArray<double> test{1};
  EXPECT_EQ(std::accumulate(test.begin(), test.end(), 0), 1);
  for (int i = 0; i < 100; ++i) {
    test.pushBack(0);
    EXPECT_EQ(std::accumulate(test.begin(), test.end(), 0), 1);
  }
}

// TEST(PersistentArrayTests)
