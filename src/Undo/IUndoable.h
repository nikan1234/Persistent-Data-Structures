#ifndef I_UNDOABLE_H
#define I_UNDOABLE_H

namespace Undo {

/// Interface for undo/redo operations
template <class Collection> class IUndoable {
public:
  [[nodiscard]] virtual Collection undo() const = 0;
  [[nodiscard]] virtual Collection redo() const = 0;
  virtual ~IUndoable() = default;
};

} // namespace Undo

#endif
