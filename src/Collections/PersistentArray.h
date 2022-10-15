#ifndef PERSISTENT_ARRAY_H
#define PERSISTENT_ARRAY_H

#include <Common/ContractExceptions.h>
#include <Common/SafeDeref.h>
#include <Undo/UndoRedoManager.h>

#include <algorithm>
#include <initializer_list>
#include <memory>

namespace Persistence {

template <typename T> class PersistentArray final : public Undo::IUndoable<PersistentArray<T>> {

  /// Base class for nodes tree implementations
  struct NodeImplBase {
    virtual ~NodeImplBase() = default;

    /// Checks is node contains element by specified index
    [[nodiscard]] virtual bool contains(std::size_t index) const = 0;
    /// Access value by index
    [[nodiscard]] virtual const T &value(std::size_t index) const = 0;
    [[nodiscard]] T &value(std::size_t index) {
      const auto &constThis = *this;
      return const_cast<T &>(constThis.value(index));
    }
    /// Swap values containing this and other nodes
    virtual void swapValues(NodeImplBase &other) { other.swapValues(*this); }
  };

  /// Root node implementation. Refers to original array.
  class RootNodeImpl final : public NodeImplBase {
  public:
    using Storage = std::vector<T>;

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

    [[nodiscard]] Storage &getStorage() { return storage_; }

  private:
    Storage storage_;
  };

  /// Change-set node implementation. Contains modified value.
  class ChangeSetNodeImpl final : public NodeImplBase {
  public:
    template <class U, class = std::enable_if_t<std::is_convertible_v<U, T>, void>>
    ChangeSetNodeImpl(const std::size_t index, U &&value)
        : modificationIndex_(index), modifiedValue_(std::forward<U>(value)) {}

    void swapValues(NodeImplBase &other) override {
      CONTRACT_EXPECT(other.contains(modificationIndex_));
      std::swap(modifiedValue_, other.value(modificationIndex_));
    }

    [[nodiscard]] bool contains(const std::size_t index) const override {
      return modificationIndex_ == index;
    }

    [[nodiscard]] const T &value(const std::size_t index) const override {
      CONTRACT_EXPECT(contains(index));
      return modifiedValue_;
    }

  private:
    std::size_t modificationIndex_;
    T modifiedValue_;
  };

  /// Persistent node with specified implementation.
  class PersistentNode {
  public:
    using Impl = std::unique_ptr<NodeImplBase>;
    using Ptr = std::shared_ptr<PersistentNode>;

    explicit PersistentNode(Impl impl, Ptr parent = nullptr)
        : impl_(std::move(impl)), parent_(std::move(parent)) {}

    [[nodiscard]] const Ptr &parent() const { return parent_; }
    [[nodiscard]] bool isRoot() const { return dynamic_cast<RootNodeImpl *>(impl_.get()); }

    [[nodiscard]] bool contains(std::size_t index) const { return impl_->contains(index); }
    [[nodiscard]] const T &value(std::size_t index) const { return impl_->value(index); }

    /// Changes node's parent
    [[maybe_unused]] Ptr reparent(Ptr newParent) {
      auto oldParent = std::move(parent_);
      parent_ = std::move(newParent);
      return oldParent;
    }

    /// Sifts up this root node to other. Applicable only if this is root node.
    void siftUpRoot(const Ptr &other) {
      CONTRACT_EXPECT(isRoot() && !other->isRoot());
      impl_->swapValues(*other->impl_);
      std::swap(impl_, other->impl_);
    }

    /// Appends value to array storage. Applicable only if this is root node.
    template <class... Args> void appendValue(Args &&...args) {
      CONTRACT_EXPECT(isRoot());
      dynamic_cast<RootNodeImpl &>(*impl_).getStorage().emplace_back(std::forward<Args>(args)...);
    }

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

  /// Creates array containing count copies of element value
  explicit PersistentArray(const std::size_t count, const T &value = T())
      : PersistentArray(PersistentNode::makeRoot(count, value), count) {}

  PersistentArray(const PersistentArray &) = delete;
  PersistentArray(PersistentArray &&) noexcept = delete;
  PersistentArray &operator=(const PersistentArray &) = delete;
  PersistentArray &operator=(PersistentArray &&) noexcept = delete;

  /// Returns size of array
  [[nodiscard]] std::size_t size() const { return size_; }

  /// Returns true is empty
  [[nodiscard]] bool empty() const { return size_ == 0u; }

  /// Returns stored value by index
  /// \param index value index in array
  [[nodiscard]] const T &value(const std::size_t index) const {
    CONTRACT_EXPECT(index < size_);

    if (!node_->contains(index))
      reRootModificationTree();
    return node_->value(index);
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
    return redo(undoRedoManager_.pushAction(Undo::createAction<PersistentArray>(undo, redo)));
  }

  /// Appends value to the end of array
  /// \param value value to append
  /// \return array containing appended value
  template <class U, class = std::enable_if_t<std::is_convertible_v<U, T>, void>>
  [[nodiscard]] PersistentArray pushBack(U &&value) const {
    auto &root = findRoot();
    if (root.contains(size_)) {
      /// Value already stored
      PersistentArray extended{node_, size_ + 1, undoRedoManager_};
      return extended.setValue(size_, value);
    }
    /// Should extend original array with value
    root.appendValue(value);

    const auto undo = [node = node_, size = size_](auto manager) {
      return PersistentArray{node, size, std::move(manager)};
    };
    const auto redo = [node = node_, size = size_ + 1](auto manager) {
      return PersistentArray{node, size, std::move(manager)};
    };
    return redo(undoRedoManager_.pushAction(Undo::createAction<PersistentArray>(undo, redo)));
  }

  /// Removes last element from array
  /// \return array without removed element
  [[nodiscard]] PersistentArray popBack() const {
    CONTRACT_EXPECT(size_ > 0);

    /// Simply decrement size and don't remove value from original array,
    /// since we don't know is this value referenced from another array or not
    const auto undo = [node = node_, size = size_](auto manager) {
      return PersistentArray{node, size, std::move(manager)};
    };
    const auto redo = [node = node_, size = size_ - 1](auto manager) {
      return PersistentArray{node, size, std::move(manager)};
    };
    return redo(undoRedoManager_.pushAction(Undo::createAction<PersistentArray>(undo, redo)));
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

  [[nodiscard]] PersistentNode &findRoot() const {
    auto foundNode = node_;
    while (foundNode && !foundNode->isRoot())
      foundNode = foundNode->parent();
    return SAFE_DEREF(foundNode);
  }

  /// Function to re-root modification tree. After re-rooting current node becomes root.
  void reRootModificationTree() const {
    std::stack<typename PersistentNode::Ptr> path;

    auto root = node_;
    while (!root->isRoot()) {
      path.push(root);
      root = root->reparent(nullptr);
    }

    /// Sift up root and reparent nodes
    while (!path.empty()) {
      root->siftUpRoot(path.top());
      root = path.top();
      path.pop();
      if (!path.empty())
        root->reparent(path.top());
    }
  }
};

} // namespace Persistence

#endif
