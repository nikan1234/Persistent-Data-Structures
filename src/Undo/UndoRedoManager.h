#ifndef I_UNDO_REDO_MANAGER_H
#define I_UNDO_REDO_MANAGER_H

#include <Common/ContractExceptions.h>
#include <Common/SafeDeref.h>
#include <Undo/IUndoable.h>

#include <array>
#include <functional>
#include <stack>

namespace Undo {

template <class Collection> class UndoRedoManager;

/// Interface for undo/redo actions used by @UndoRedoManager
template <class Collection> class IUndoRedoAction {
public:
  virtual ~IUndoRedoAction() = default;

protected:
  friend class UndoRedoManager<Collection>;
  [[nodiscard]] virtual Collection applyUndo(UndoRedoManager<Collection>) const = 0;
  [[nodiscard]] virtual Collection applyRedo(UndoRedoManager<Collection>) const = 0;
};

/// Default realization for undo/redo action.
template <class Collection, class UndoHandler, class RedoHandler>
class UndoRedoAction final : public IUndoRedoAction<Collection> {
public:
  UndoRedoAction(UndoHandler undo, RedoHandler redo)
      : undo_(std::move(undo)), redo_(std::move(redo)) {}

protected:
  [[nodiscard]] Collection applyUndo(UndoRedoManager<Collection> manager) const override {
    return undo_(std::move(manager));
  }

  [[nodiscard]] Collection applyRedo(UndoRedoManager<Collection> manager) const override {
    return redo_(std::move(manager));
  }

private:
  UndoHandler undo_;
  RedoHandler redo_;
};

/// Shortcut to create undo/redo action
template <class Collection, class UndoHandler, class RedoHandler>
[[nodiscard]] auto createAction(UndoHandler undo, RedoHandler redo) {
  return std::make_shared<UndoRedoAction<Collection, UndoHandler, RedoHandler>>(std::move(undo),
                                                                                std::move(redo));
}

/// Class to store and manage undo/redo operations
template <class Collection> class UndoRedoManager final : public IUndoable<Collection> {
public:
  using ActionPtr = std::shared_ptr<IUndoRedoAction<Collection>>;

  /// Adds new undo/redo action. Clears redo stack.
  /// \param undoRedoAction
  /// Returns new manager and leaves current manager unchanged.
  [[nodiscard]] UndoRedoManager pushAction(ActionPtr undoRedoAction) const {
    CONTRACT_EXPECT(undoRedoAction);
    auto managerCopy = *this;
    managerCopy.undoStack_.push(undoRedoAction);
    managerCopy.redoStack_ = {};
    return managerCopy;
  }

  /// Checks that undo stack in not empty
  [[nodiscard]] bool hasUndo() const { return !undoStack_.empty(); }
  /// Checks that redo stack in not empty
  [[nodiscard]] bool hasRedo() const { return !redoStack_.empty(); }

  /// Undoes recent action
  [[nodiscard]] Collection undo() const override {
    CONTRACT_EXPECT(hasUndo());
    auto copyManager = *this;
    auto &recentAction = SAFE_DEREF(undoStack_.top());
    moveRecentAction(copyManager.undoStack_, copyManager.redoStack_);
    return recentAction.applyUndo(std::move(copyManager));
  }
  /// Redoes recent action
  [[nodiscard]] Collection redo() const override {
    CONTRACT_EXPECT(hasRedo());
    auto copyManager = *this;
    auto &recentAction = SAFE_DEREF(redoStack_.top());
    moveRecentAction(copyManager.redoStack_, copyManager.undoStack_);
    return recentAction.applyRedo(std::move(copyManager));
  }

private:
  using UndoRedoStack = std::stack<ActionPtr>;

  static void moveRecentAction(UndoRedoStack &source, UndoRedoStack &target) {
    auto action = source.top();
    source.pop();
    target.push(std::move(action));
  }

  UndoRedoStack undoStack_;
  UndoRedoStack redoStack_;
};

} // namespace Undo

#endif
