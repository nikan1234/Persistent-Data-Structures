#include <Undo/UndoRedoManager.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace testing;
using namespace Undo;

namespace {

struct FakeUndoableCollection final {
  enum class Type { ReturnedFromUndo, ReturnedFromRedo };

  Type type = Type::ReturnedFromUndo;
  UndoRedoManager<FakeUndoableCollection> manager;
};

class UndoRedoActionMock final : public IUndoRedoAction<FakeUndoableCollection> {
public:
  static auto create() { return std::make_shared<UndoRedoActionMock>(); }

  void expectUndo() const {
    EXPECT_CALL(*this, applyUndo(_)).WillOnce(Invoke([](auto manager) {
      return FakeUndoableCollection{FakeUndoableCollection::Type::ReturnedFromUndo,
                                    std::move(manager)};
    }));
  }

  void expectRedo() const {
    EXPECT_CALL(*this, applyRedo(_)).WillOnce(Invoke([](auto manager) {
      return FakeUndoableCollection{FakeUndoableCollection::Type::ReturnedFromRedo,
                                    std::move(manager)};
    }));
  }

protected:
  MOCK_METHOD(FakeUndoableCollection, applyUndo, (UndoRedoManager<FakeUndoableCollection>),
              (const, override));
  MOCK_METHOD(FakeUndoableCollection, applyRedo, (UndoRedoManager<FakeUndoableCollection>),
              (const, override));
};
} // namespace

TEST(UndoRedoManagerTest, TestDefaultConstructed) {
  const UndoRedoManager<FakeUndoableCollection> manager;
  EXPECT_FALSE(manager.hasUndo());
  EXPECT_FALSE(manager.hasRedo());
}

TEST(UndoRedoManagerTest, TestPushAction) {
  const UndoRedoManager<FakeUndoableCollection> manager;
  const auto withUndo = manager.pushUndo(UndoRedoActionMock::create());

  EXPECT_FALSE(manager.hasUndo());
  EXPECT_FALSE(manager.hasRedo());
  EXPECT_TRUE(withUndo.hasUndo());
  EXPECT_FALSE(withUndo.hasRedo());
}

TEST(UndoRedoManagerTest, TestUndoRedo) {
  auto action = UndoRedoActionMock::create();
  action->expectUndo();
  action->expectRedo();

  const auto originalManager =
      UndoRedoManager<FakeUndoableCollection>{}.pushUndo(std::move(action));

  // Collection returned with undo() contains manager with redo action.
  const auto [undidType, undidManager] = originalManager.undo();
  EXPECT_EQ(undidType, FakeUndoableCollection::Type::ReturnedFromUndo);
  EXPECT_FALSE(undidManager.hasUndo());
  EXPECT_TRUE(undidManager.hasRedo());

  // Collection returned with redo() contains manager with undo action.
  const auto [redidType, redidManager] = undidManager.redo();
  EXPECT_EQ(redidType, FakeUndoableCollection::Type::ReturnedFromRedo);
  EXPECT_TRUE(redidManager.hasUndo());
  EXPECT_FALSE(redidManager.hasRedo());
}

TEST(UndoRedoManagerTest, TestPushActionWithRedo) {
  auto action = UndoRedoActionMock::create();
  action->expectUndo();
  const auto originalManager =
      UndoRedoManager<FakeUndoableCollection>{}.pushUndo(std::move(action));

  const auto [undidType, undidManager] = originalManager.undo();
  EXPECT_EQ(undidType, FakeUndoableCollection::Type::ReturnedFromUndo);
  EXPECT_FALSE(undidManager.hasUndo());
  EXPECT_TRUE(undidManager.hasRedo());

  // If we add undo action, then redo actions should be removed
  const auto otherManager = undidManager.pushUndo(UndoRedoActionMock::create());
  EXPECT_TRUE(otherManager.hasUndo());
  EXPECT_FALSE(otherManager.hasRedo());
}
