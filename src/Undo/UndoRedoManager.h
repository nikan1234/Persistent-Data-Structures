#ifndef I_UNDO_REDO_MANAGER_H
#define I_UNDO_REDO_MANAGER_H

#include <Undo/IUndoable.h>

#include <array>
#include <functional>
#include <stack>

namespace Undo {

enum class ActionType { Undo = 0, Redo, Count };

constexpr ActionType operator!(const ActionType type) {
  return type == ActionType::Undo ? ActionType::Redo : ActionType::Undo;
}

/// Class to store and manage undo/redo operations
template <class Collection> class UndoRedoManager final : public IUndoable<Collection> {
public:
  template <class T>
  using UndoRedoStorage = std::array<T, static_cast<std::size_t>(ActionType::Count)>;
  using UndoRedoHandler = std::function<Collection(UndoRedoManager<Collection>)>;

  class Action {
  public:
    Action(UndoRedoHandler undoHandler, UndoRedoHandler redoHandler)
        : handlers_({std::move(undoHandler), std::move(redoHandler)}) {}

    [[nodiscard]] const UndoRedoHandler &getHandler(const ActionType type) const {
      return handlers_.at(static_cast<std::size_t>(type));
    }

  private:
    UndoRedoStorage<UndoRedoHandler> handlers_;
  };

  /// Adds new undo/redo action. Clears redo stack.
  /// \param undoRedoAction
  /// Returns new manager and leaves current manager unchanged.
  [[nodiscard]] UndoRedoManager pushUndoAction(const Action &undoRedoAction) const {
    auto managerCopy = *this;
    managerCopy.undoStack_.push(undoRedoAction);
    managerCopy.redoStack_ = {};
    return managerCopy;
  }

  /// Checks that undo stack in not empty
  [[nodiscard]] bool hasUndo() const { return !getStack(ActionType::Undo).empty(); }
  /// Checks that redo stack in not empty
  [[nodiscard]] bool hasRedo() const { return !getStack(ActionType::Redo).empty(); }

  /// Undoes recent action
  [[nodiscard]] Collection undo() const override { return handleUndoRedo(ActionType::Undo); }
  /// Redoes recent action
  [[nodiscard]] Collection redo() const override { return handleUndoRedo(ActionType::Redo); }

private:
  using UndoRedoStack = std::stack<Action>;

  [[nodiscard]] const UndoRedoStack &getStack(const ActionType type) const {
    return undoRedoStacks_.at(static_cast<std::size_t>(type));
  }

  [[nodiscard]] UndoRedoStack &getStack(const ActionType type) {
    const auto &constThis = *this;
    return const_cast<UndoRedoStack &>(constThis.getStack(type));
  }

  [[nodiscard]] Collection handleUndoRedo(const ActionType type) const {
    auto managerCopy = *this;
    auto &sourceStack = managerCopy.getStack(type);
    auto &targetStack = managerCopy.getStack(!type);

    const auto action = sourceStack.top();
    sourceStack.pop();
    targetStack.push(action);
    return action.getHandler(type)(std::move(managerCopy));
  }

  UndoRedoStorage<UndoRedoStack> undoRedoStacks_ = {};
};

} // namespace Undo

#endif
