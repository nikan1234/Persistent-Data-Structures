#ifndef PERSISTENT_ARRAY_H
#define PERSISTENT_ARRAY_H

#include <Common/ContractExceptions.h>
#include <Common/SafeDeref.h>
#include <Undo/UndoRedoManager.h>

#include <algorithm>
#include <initializer_list>
#include <memory>

namespace Persistence {

template <class T> class PersistentArrayIterator;

/// @PersistentArray
/// Persistent array implementation.
template <class T> class PersistentArray final : public Undo::IUndoable<PersistentArray<T>> {

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
  using Value = T;
  using Iterator = PersistentArrayIterator<T>;
  using ReverseIterator = std::reverse_iterator<PersistentArrayIterator<T>>;

  /// Creates empty array
  PersistentArray() = default;

  /// Creates array containing specified values
  /// \param values values to store
  PersistentArray(const std::initializer_list<T> &values)
      : PersistentArray(values.size(),
                        !empty(values) ? PersistentNode::makeRoot(values) : nullptr) {}

  /// Creates array containing count copies of element value
  explicit PersistentArray(const std::size_t count, const T &value = T())
      : PersistentArray(count, count ? PersistentNode::makeRoot(count, value) : nullptr) {}

  /// Moves array from other to this
  PersistentArray(PersistentArray &&other) noexcept
      : PersistentArray(other.size_, std::move(other.node_), std::move(other.undoRedoManager_)) {
    other.size_ = 0u;
  }

  /// Makes swallow copy of other array
  PersistentArray &operator=(PersistentArray &&other) noexcept {
    if (this == &other)
      return *this;

    size_ = other.size_;
    other.size_ = 0;

    node_ = std::move(other.node_);
    undoRedoManager_ = std::move(other.undoRedoManager_);
    return *this;
  }

  /// Returns size of array
  [[nodiscard]] std::size_t size() const { return size_; }

  /// Returns true is empty
  [[nodiscard]] bool empty() const { return size_ == 0u; }

  /// Access to first element
  [[nodiscard]] const T &front() const {
    CONTRACT_EXPECT(!empty());
    return value(0);
  }

  /// Access to last element
  [[nodiscard]] const T &back() const {
    CONTRACT_EXPECT(!empty());
    return value(size_ - 1);
  }

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

    const auto undo = [size = size_, node = node_](auto manager) {
      return PersistentArray{size, node, std::move(manager)};
    };
    const auto redo = [size = size_, node = std::move(changeSetNode)](auto manager) {
      return PersistentArray{size, node, std::move(manager)};
    };
    return redo(undoRedoManager_.pushAction(Undo::createAction<PersistentArray>(undo, redo)));
  }

  /// Appends value to the end of array
  /// \param value value to append
  /// \return array containing appended value
  template <class U, class = std::enable_if_t<std::is_convertible_v<U, T>, void>>
  [[nodiscard]] PersistentArray pushBack(U &&value) const {
    auto &root = findOrCreateRoot();
    if (root.contains(size_)) {
      /// Value already stored in original array
      PersistentArray extended{size_ + 1, node_, undoRedoManager_};
      return extended.setValue(size_, value);
    }
    /// Should extend original array with value
    root.appendValue(value);

    const auto undo = [size = size_, node = node_](auto manager) {
      return PersistentArray{size, node, std::move(manager)};
    };
    const auto redo = [size = size_ + 1, node = node_](auto manager) {
      return PersistentArray{size, node, std::move(manager)};
    };
    return redo(undoRedoManager_.pushAction(Undo::createAction<PersistentArray>(undo, redo)));
  }

  /// Removes last element from array
  /// \return array without removed element
  [[nodiscard]] PersistentArray popBack() const {
    CONTRACT_EXPECT(!empty());

    /// Simply decrement size and don't remove value from original array,
    /// since we don't know is this value referenced from another array or not
    const auto undo = [size = size_, node = node_](auto manager) {
      return PersistentArray{size, node, std::move(manager)};
    };
    const auto redo = [size = size_ - 1, node = node_](auto manager) {
      return PersistentArray{size, node, std::move(manager)};
    };
    return redo(undoRedoManager_.pushAction(Undo::createAction<PersistentArray>(undo, redo)));
  }

  [[nodiscard]] Iterator begin() const { return Iterator{*this}; }
  [[nodiscard]] Iterator end() const { return Iterator{*this, size()}; }
  [[nodiscard]] ReverseIterator rbegin() const { return ReverseIterator{end()}; }
  [[nodiscard]] ReverseIterator rend() const { return ReverseIterator{begin()}; }

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
  mutable typename PersistentNode::Ptr node_; // lazy initialization
  Undo::UndoRedoManager<PersistentArray> undoRedoManager_;

  explicit PersistentArray(const std::size_t size, typename PersistentNode::Ptr node,
                           Undo::UndoRedoManager<PersistentArray> undoRedoManager = {})
      : size_(size), node_(std::move(node)), undoRedoManager_(std::move(undoRedoManager)) {}

  [[nodiscard]] PersistentNode &findOrCreateRoot() const {
    if (!node_)
      return SAFE_DEREF(node_ = PersistentNode::makeRoot());

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
      root->reparent(std::move(path.top()));
      path.pop();
      root = root->parent();
    }
  }
};

/// Iterator class for @PersistentArray
template <class T> class PersistentArrayIterator final {
  friend class PersistentArray<T>;
  using PositionIndex = std::size_t;

public:
  using Value = typename PersistentArray<T>::Value;
  using Reference = const Value &;
  using Pointer = const Value *;
  using Difference = std::make_signed_t<PositionIndex>;

  PersistentArrayIterator(const PersistentArrayIterator &) = default;
  PersistentArrayIterator &operator=(const PersistentArrayIterator &) = default;

  /// Access to value which iterator points to.
  [[nodiscard]] Reference operator*() const { return target_->value(currentIndex_); }
  [[nodiscard]] Pointer operator->() const { return &target_->value(currentIndex_); }

  PersistentArrayIterator &operator++() {
    CONTRACT_EXPECT(currentIndex_ < target_->size());
    ++currentIndex_;
    return *this;
  }

  PersistentArrayIterator &operator--() {
    CONTRACT_EXPECT(currentIndex_ > 0);
    --currentIndex_;
    return *this;
  }

  PersistentArrayIterator operator++(int) {
    PersistentArrayIterator tmp = *this;
    ++*this;
    return *this;
  }

  PersistentArrayIterator &operator--(int) {
    PersistentArrayIterator tmp = *this;
    --*this;
    return *this;
  }

  PersistentArrayIterator &operator+=(const Difference difference) {
    CONTRACT_EXPECT(verifyOffset(difference));
    currentIndex_ += difference;
    return *this;
  }

  PersistentArrayIterator &operator-=(const Difference difference) { return *this += -difference; }

  [[nodiscard]] PersistentArrayIterator operator+(const Difference difference) {
    PersistentArrayIterator tmp = *this;
    return *this += difference;
  }

  [[nodiscard]] PersistentArrayIterator operator-(const Difference difference) {
    PersistentArrayIterator tmp = *this;
    return *this -= difference;
  }

  [[nodiscard]] Difference operator-(const PersistentArrayIterator &other) const {
    return currentIndex_ - other.currentIndex_;
  }

  [[nodiscard]] bool operator==(const PersistentArrayIterator &other) const {
    CONTRACT_EXPECT(verifyCompatibility(other));
    return currentIndex_ == other.currentIndex_;
  }

  [[nodiscard]] bool operator!=(const PersistentArrayIterator &other) const {
    return !(currentIndex_ == other.currentIndex_);
  }

  [[nodiscard]] bool operator<(const PersistentArrayIterator &other) const {
    CONTRACT_EXPECT(verifyCompatibility(other));
    return currentIndex_ < other.currentIndex_;
  }

  [[nodiscard]] bool operator>(const PersistentArrayIterator &other) const { return other < *this; }

  [[nodiscard]] bool operator<=(const PersistentArrayIterator &other) const {
    return !(this > other);
  }

  [[nodiscard]] bool operator>=(const PersistentArrayIterator &other) const {
    return !(this < other);
  }

private:
  [[nodiscard]] bool verifyOffset(const Difference difference) const noexcept {
    if (difference > 0)
      return difference <= target_->size() - currentIndex_;
    return -difference <= currentIndex_;
  }

  [[nodiscard]] bool verifyCompatibility(const PersistentArrayIterator &other) const noexcept {
    return target_ == other.target_;
  }

  explicit PersistentArrayIterator(const PersistentArray<T> &target,
                                   const PositionIndex currentIndex = 0u)
      : target_(&target), currentIndex_(currentIndex) {}

  const PersistentArray<T> *target_;
  PositionIndex currentIndex_;
};

} // namespace Persistence

namespace std {
/// Specialization for std::iterator_traits
template <class T> struct iterator_traits<Persistence::PersistentArrayIterator<T>> {
  using iterator_category = random_access_iterator_tag;
  using value_type = typename Persistence::PersistentArrayIterator<T>::Value;
  using pointer = typename Persistence::PersistentArrayIterator<T>::Pointer;
  using reference = typename Persistence::PersistentArrayIterator<T>::Reference;
  using difference_type = typename Persistence::PersistentArrayIterator<T>::Difference;
};
} // namespace std

#endif
