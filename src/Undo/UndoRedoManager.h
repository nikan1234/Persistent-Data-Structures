#ifndef I_UNDO_REDO_MANAGER_H
#define I_UNDO_REDO_MANAGER_H

#include <Common/ContractExceptions.h>
#include <Common/SafeDeref.h>
#include <Undo/IUndoable.h>

#include <array>
#include <functional>

namespace Undo {

template <class Collection> class UndoRedoManager;

/// Interface for undo/redo actions used by @UndoRedoManager
template <class Collection> class IUndoRedoAction {
public:
  using Ptr = std::shared_ptr<IUndoRedoAction>;
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
  using ActionPtr = typename IUndoRedoAction<Collection>::Ptr;

  /// Persistent implementation for undo/redo stack for efficient memory usage.
  class UndoRedoStack final {
    struct Entry {
      using Ptr = std::shared_ptr<Entry>;
      explicit Entry(ActionPtr action, Ptr next = nullptr)
          : action(std::move(action)), next(std::move(next)) {}

      ActionPtr action;
      Ptr next;
    };

  public:
    UndoRedoStack(typename Entry::Ptr top = nullptr) : top_(std::move(top)) {}
    UndoRedoStack(UndoRedoStack &&) noexcept = default;
    UndoRedoStack &operator=(UndoRedoStack &&) noexcept = default;

    ~UndoRedoStack() {
      while (top_)
        top_ = std::move(top_->next);
    }

    /// Pop action from stack
    [[nodiscard]] UndoRedoStack pop() const {
      return top_ ? UndoRedoStack{top_->next} : UndoRedoStack{};
    }

    /// Push action into stack
    [[nodiscard]] UndoRedoStack push(ActionPtr action) const {
      return UndoRedoStack{std::make_shared<Entry>(std::move(action), top_)};
    }

    /// Returns true if empty
    [[nodiscard]] bool empty() const { return !top_; }

    /// Returns top action
    [[nodiscard]] const auto &top() const {
      CONTRACT_EXPECT(!empty());
      return top_->action;
    }

  private:
    typename Entry::Ptr top_;
  };

  /// Manager contains two stacks for undo and redo actions
  UndoRedoManager(UndoRedoStack undoStack, UndoRedoStack redoStack)
      : undoStack_(std::move(undoStack)), redoStack_(std::move(redoStack)) {}

public:
  /// Creates new manager
  UndoRedoManager() = default;

  /// Adds new undo/redo action. Clears redo stack.
  /// \param undoRedoAction
  /// Returns new manager and leaves current manager unchanged.
  [[nodiscard]] UndoRedoManager pushUndo(ActionPtr undoRedoAction) const {
    CONTRACT_EXPECT(undoRedoAction);
    return UndoRedoManager{undoStack_.push(std::move(undoRedoAction)), {}};
  }

  /// Checks that undo stack in not empty
  [[nodiscard]] bool hasUndo() const { return !undoStack_.empty(); }
  /// Checks that redo stack in not empty
  [[nodiscard]] bool hasRedo() const { return !redoStack_.empty(); }

  /// Undoes recent action
  [[nodiscard]] Collection undo() const override {
    CONTRACT_EXPECT(hasUndo());
    auto &recentAction = SAFE_DEREF(undoStack_.top());
    auto [undo, redo] = moveRecentAction(undoStack_, redoStack_);
    return recentAction.applyUndo(UndoRedoManager{std::move(undo), std::move(redo)});
  }

  /// Redoes recent action
  [[nodiscard]] Collection redo() const override {
    CONTRACT_EXPECT(hasRedo());
    auto &recentAction = SAFE_DEREF(redoStack_.top());
    auto [redo, undo] = moveRecentAction(redoStack_, undoStack_);
    return recentAction.applyRedo(UndoRedoManager{std::move(undo), std::move(redo)});
  }

private:
  static auto moveRecentAction(const UndoRedoStack &source, const UndoRedoStack &target) {
    auto action = source.top();
    return std::make_pair(source.pop(), target.push(std::move(action)));
  }

  UndoRedoStack undoStack_;
  UndoRedoStack redoStack_;
};

} // namespace Undo

#endif
