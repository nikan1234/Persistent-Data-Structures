#ifndef PERSISTENT_ARRAY_H
#define PERSISTENT_ARRAY_H

#include <Common/ContractExceptions.h>
#include <Undo/UndoRedoManager.h>

#include <initializer_list>
#include <memory>
#include <stdexcept>

namespace Persistence {

template <typename T> class PersistentArray final : public Undo::IUndoable<PersistentArray<T>> {
  // Base class for nodes tree implementations
  struct INodeImpl {
    virtual ~INodeImpl() = default;
    [[nodiscard]] virtual const T &value(std::size_t index) const = 0;
    [[nodiscard]] virtual bool contains(std::size_t index) const = 0;
  };

  // Root node implementation. Refers to original array.
  class RootNodeImpl final : public INodeImpl {
  public:
    template <class... Args>
    explicit RootNodeImpl(Args &&...args) : storage_(std::forward<Args>(args)...) {}

    template <class... Args> void append(Args &&...args) {
      storage_.emplace_back(std::forward<Args>(args)...);
    }

    [[nodiscard]] bool contains(const std::size_t index) const override {
      return index < storage_.size();
    }

    [[nodiscard]] const T &value(const std::size_t index) const override {
      return storage_.at(index);
    }

  private:
    std::vector<T> storage_;
  };

  // Change-set node implementation. Contains modified value.
  class ChangeSetNodeImpl final : public INodeImpl {
  public:
    template <class U, class = std::enable_if_t<std::is_convertible_v<U, T>, void>>
    ChangeSetNodeImpl(const std::size_t index, U &&value)
        : modification_index(index), modified_value(std::forward<U>(value)) {}

    [[nodiscard]] bool contains(const std::size_t index) const override {
      return modification_index == index;
    }

    [[nodiscard]] const T &value(const std::size_t index) const override {
      if (contains(index))
        return modified_value;
      throw std::runtime_error("Wrong index");
    }

    std::size_t modification_index;
    T modified_value;
  };

  // Persistent node with specified implementation.
  class PersistentNode {
  public:
    using Impl = std::unique_ptr<INodeImpl>;
    using Ptr = std::shared_ptr<PersistentNode>;

    explicit PersistentNode(Impl impl, Ptr parent = nullptr)
        : impl_(std::move(impl)), parent_(std::move(parent)) {}

    [[nodiscard]] const Impl &getImpl() const { return impl_; }

    [[nodiscard]] const Ptr &getParent() const { return parent_; }

    [[nodiscard]] bool contains(std::size_t index) const { return impl_->contains(index); }
    [[nodiscard]] const T &value(std::size_t index) const { return impl_->value(index); }

    template <class... Args> [[nodiscard]] static Ptr makeRoot(Args &&...args) {
      return std::make_shared<PersistentNode>(
          std::make_unique<RootNodeImpl>(std::forward<Args>(args)...));
    }

    template <class U>
    [[nodiscard]] static Ptr makeChangeSet(const std::size_t index, U &&value, Ptr parent) {
      return std::make_shared<PersistentNode>(
          std::make_unique<ChangeSetNodeImpl>(index, std::forward<U>(value)), std::move(parent));
    }

  private:
    Impl impl_;
    Ptr parent_;
  };

public:
  /// Creates empty array
  PersistentArray() : PersistentArray(PersistentNode::makeRoot(), 0u) {}

  /// Creates array containing specified values
  /// \param values values to store
  PersistentArray(const std::initializer_list<T> &values)
      : PersistentArray(PersistentNode::makeRoot(values), values.size()) {}

  PersistentArray(const PersistentArray &) = delete;
  PersistentArray(PersistentArray &&) noexcept = delete;
  PersistentArray &operator=(const PersistentArray &) = delete;
  PersistentArray &operator=(PersistentArray &&) noexcept = delete;

  /// Returns size of array
  [[nodiscard]] std::size_t size() const { return size_; }

  /// Returns stored value by index
  /// \param index value index in array
  [[nodiscard]] const T &value(const std::size_t index) const {
    CONTRACT_EXPECT(index < size_);

    auto foundNode = node_;
    while (foundNode && !foundNode->contains(index))
      foundNode = foundNode->getParent();

    CONTRACT_ENSURE(foundNode);
    return foundNode->value(index);
  }

  /// Changes array value for specified index
  /// \param index array index
  /// \param value new value
  /// \return array containing new value
  template <class U, class = std::enable_if_t<std::is_convertible_v<U, T>, void>>
  [[nodiscard]] PersistentArray setValue(const std::size_t index, U &&value) const {
    CONTRACT_EXPECT(index < size_);

    auto changeSetNode = PersistentNode::makeChangeSet(index, std::forward<U>(value), node_);

    const auto undo = [node = node_, size = size_](auto manager) {
      return PersistentArray{node, size, std::move(manager)};
    };
    const auto redo = [node = std::move(changeSetNode), size = size_](auto manager) {
      return PersistentArray{node, size, std::move(manager)};
    };

    return redo(undoRedoManager_.pushUndoAction({undo, redo}));
  }

  /// Appends value to the end of array
  /// \param value value to append
  /// \return array containing appended value
  template <class U, class = std::enable_if_t<std::is_convertible_v<U, T>, void>>
  [[nodiscard]] PersistentArray push_back(U &&value) const {

    auto root = node_;
    while (root->getParent())
      root = root->getParent();

    if (root->contains(size_)) {
      // Value already stored
      PersistentArray extended{node_, size_ + 1, undoRedoManager_};
      return extended.setValue(size_, value);
    }

    // Should extend original array with value
    auto &rootImpl = SAFE_DEREF(dynamic_cast<RootNodeImpl *>(root->getImpl().get()));
    rootImpl.append(value);

    const auto undo = [node = node_, size = size_](auto manager) {
      return PersistentArray{node, size, std::move(manager)};
    };
    const auto redo = [node = node_, size = size_ + 1](auto manager) {
      return PersistentArray{node, size, std::move(manager)};
    };
    return redo(undoRedoManager_.pushUndoAction({undo, redo}));
  }

  /// Removes last element from array
  /// \returns array without removed element
  [[nodiscard]] PersistentArray pop_back() const {
    CONTRACT_EXPECT(size_ > 0);

    // Simply decrement size and don't remove value from original array,
    // since we don't know is this value referenced from another array or not
    const auto undo = [node = node_, size = size_](auto manager) {
      return PersistentArray{node, size, std::move(manager)};
    };
    const auto redo = [node = node_, size = size_ - 1](auto manager) {
      return PersistentArray{node, size, std::move(manager)};
    };
    return redo(undoRedoManager_.pushUndoAction({undo, redo}));
  }

  /// Undoes last modification operation
  [[nodiscard]] PersistentArray undo() const override {
    CONTRACT_EXPECT(undoRedoManager_.hasUndo());
    return undoRedoManager_.undo();
  }

  /// Redoes last modification operation
  [[nodiscard]] PersistentArray redo() const override {
    CONTRACT_EXPECT(undoRedoManager_.hasRedo());
    return undoRedoManager_.redo();
  }

private:
  std::size_t size_ = 0;
  typename PersistentNode::Ptr node_;
  Undo::UndoRedoManager<PersistentArray> undoRedoManager_;

  PersistentArray(typename PersistentNode::Ptr node, const std::size_t size,
                  Undo::UndoRedoManager<PersistentArray> undoRedoManager = {})
      : size_(size), node_(std::move(node)), undoRedoManager_(std::move(undoRedoManager)) {}
};

} // namespace Persistence

#endif
