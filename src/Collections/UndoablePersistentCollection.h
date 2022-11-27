#ifndef UNDOABLE_PERSISTENT_COLLECTION_H
#define UNDOABLE_PERSISTENT_COLLECTION_H

#include <Undo/UndoRedoManager.h>

namespace Persistence {
template <class Collection>
class UndoablePersistentCollection : public Undo::IUndoable<Collection> {
  using UndoRedoManager = Undo::UndoRedoManager<Collection>;

public:
  explicit UndoablePersistentCollection(UndoRedoManager undoRedoManager = {})
      : undoRedoManager_(std::move(undoRedoManager)) {}

  UndoablePersistentCollection(UndoablePersistentCollection &&) noexcept = default;
  UndoablePersistentCollection &operator=(UndoablePersistentCollection &&) noexcept = default;

  /// Undoes last modification operation
  [[nodiscard]] Collection undo() const override {
    CONTRACT_EXPECT(undoRedoManager_.hasUndo());
    return undoRedoManager_.undo();
  }

  /// Redoes last modification operation
  [[nodiscard]] Collection redo() const override {
    CONTRACT_EXPECT(undoRedoManager_.hasRedo());
    return undoRedoManager_.redo();
  }

protected:
  [[nodiscard]] const auto &undoManager() const { return undoRedoManager_; }
  [[nodiscard]] auto &undoManager() { return undoRedoManager_; }

private:
  UndoRedoManager undoRedoManager_;
};
} // namespace Persistence

#endif
