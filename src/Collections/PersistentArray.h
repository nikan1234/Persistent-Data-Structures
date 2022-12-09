#ifndef PERSISTENT_ARRAY_H
#define PERSISTENT_ARRAY_H

#include "UndoablePersistentCollection.h"

#include <Common/ContractExceptions.h>
#include <Common/SafeDeref.h>

#include <initializer_list>
#include <memory>
#include <stack>

namespace Persistence {

template <class T> class PersistentArrayIterator;

/// @PersistentArray
/// Persistent array implementation.
/// A more efficient way of implementing Fully Persistent Arrays is by using a single instance of an
/// in-memory Array and in conjunction use a tree of modifications. Instead of storing all the
/// versions separately, Backer's trick allows us to compute any version of the array by replaying
/// all the changes asked for.
template <class T>
class PersistentArray final : public UndoablePersistentCollection<PersistentArray<T>> {

  using Base = UndoablePersistentCollection<PersistentArray>;

  using ValuePtr = std::unique_ptr<const T>;
  using ValuePtrStorage = std::vector<ValuePtr>;

  /// Base class for nodes tree implementations
  struct NodeImplBase {
    virtual ~NodeImplBase() = default;

    /// Checks is node contains element by specified index
    [[nodiscard]] virtual bool contains(std::size_t index) const = 0;
    /// Access value by index
    [[nodiscard]] virtual const ValuePtr &value(std::size_t index) const = 0;
    [[nodiscard]] ValuePtr &value(std::size_t index) {
      const auto &constThis = *this;
      return const_cast<ValuePtr &>(constThis.value(index));
    }
    /// Swap values containing this and other nodes
    virtual void swapValues(NodeImplBase &other) { other.swapValues(*this); }
  };

  /// Root node implementation. Refers to original array.
  class RootNodeImpl final : public NodeImplBase {
  public:
    template <class... Args>
    explicit RootNodeImpl(Args &&...args) : storage_(std::forward<Args>(args)...) {}

    [[nodiscard]] bool contains(const std::size_t index) const override {
      return index < storage_.size();
    }

    [[nodiscard]] const ValuePtr &value(const std::size_t index) const override {
      return storage_.at(index);
    }

    [[nodiscard]] auto &getStorage() { return storage_; }

  private:
    ValuePtrStorage storage_;
  };

  /// Change-set node implementation. Contains modified value.
  class ChangeSetNodeImpl final : public NodeImplBase {
  public:
    template <class... Args>
    explicit ChangeSetNodeImpl(const std::size_t index, Args &&...args)
        : modificationIndex_(index),
          modifiedValue_(std::make_unique<T>(std::forward<Args>(args)...)) {}

    void swapValues(NodeImplBase &other) override {
      CONTRACT_EXPECT(other.contains(modificationIndex_));
      modifiedValue_.swap(other.value(modificationIndex_));
    }

    [[nodiscard]] bool contains(const std::size_t index) const override {
      return modificationIndex_ == index;
    }

    [[nodiscard]] const ValuePtr &value(const std::size_t index) const override {
      CONTRACT_EXPECT(contains(index));
      return modifiedValue_;
    }

  private:
    std::size_t modificationIndex_;
    ValuePtr modifiedValue_;
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
    [[nodiscard]] const ValuePtr &value(std::size_t index) const { return impl_->value(index); }

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
    template <class... Args> void extend(Args &&...args) {
      CONTRACT_EXPECT(isRoot());
      dynamic_cast<RootNodeImpl &>(*impl_).getStorage().emplace_back(
          std::make_unique<T>(std::forward<Args>(args)...));
    }

    template <class... Args> [[nodiscard]] static Ptr makeRoot(Args &&...args) {
      return std::make_shared<PersistentNode>(
          std::make_unique<RootNodeImpl>(std::forward<Args>(args)...));
    }

    template <class... Args>
    [[nodiscard]] static Ptr makeChangeSet(Ptr parent, const std::size_t index, Args &&...args) {
      return std::make_shared<PersistentNode>(
          std::make_unique<ChangeSetNodeImpl>(index, std::forward<Args>(args)...),
          std::move(parent));
    }

  private:
    Impl impl_;
    Ptr parent_;
  };

public:
  using value_type = T;
  using iterator = PersistentArrayIterator<value_type>;
  using reverse_iterator = std::reverse_iterator<PersistentArrayIterator<value_type>>;

  /// Creates empty array.
  PersistentArray() = default;

  /// Creates array containing specified values.
  /// \param values values to store
  PersistentArray(const std::initializer_list<value_type> &values)
      : PersistentArray(values.size(), nullptr) {
    ValuePtrStorage storage;
    storage.reserve(size_);
    std::transform(values.begin(), values.end(), std::back_inserter(storage),
                   [](const value_type &value) { return std::make_unique<value_type>(value); });
    node_ = PersistentNode::makeRoot(std::move(storage));
  }

  /// Creates array containing count copies of element value
  explicit PersistentArray(const std::size_t count, const value_type &value = value_type())
      : PersistentArray(count, nullptr) {
    ValuePtrStorage storage;
    storage.reserve(size_);
    std::generate_n(std::back_inserter(storage), count,
                    [&value] { return std::make_unique<value_type>(value); });
    node_ = PersistentNode::makeRoot(std::move(storage));
  }

  /// Constructs array from other.
  PersistentArray(PersistentArray &&other) noexcept
      : PersistentArray(other.size_, std::move(other.node_), std::move(other.undoManager())) {
    other.size_ = 0u;
  }

  /// Moves array from other to this.
  PersistentArray &operator=(PersistentArray &&other) noexcept {
    if (this == &other)
      return *this;

    node_ = std::move(other.node_);
    size_ = other.size_;
    other.size_ = 0;
    static_cast<Base &>(*this) = std::move(other);
    return *this;
  }

  ~PersistentArray() override {
    while (node_ && node_.use_count() == 1)
      node_ = node_->reparent(nullptr);
  }

  /// Returns size of array.
  /// Complexity: constant.
  [[nodiscard]] std::size_t size() const noexcept { return size_; }

  /// Returns true is empty.
  /// Complexity: constant.
  [[nodiscard]] bool empty() const noexcept { return size_ == 0u; }

  /// Access to first element.
  /// Complexity: constant.
  [[nodiscard]] const value_type &front() const {
    CONTRACT_EXPECT(!empty());
    return value(0);
  }

  /// Access to last element.
  /// Complexity: constant.
  [[nodiscard]] const value_type &back() const {
    CONTRACT_EXPECT(!empty());
    return value(size_ - 1);
  }

  /// Returns stored value by index.
  /// Complexity: amortized constant.
  /// \param index value index in array
  [[nodiscard]] const value_type &value(const std::size_t index) const {
    CONTRACT_EXPECT(index < size_);
    if (!node_->contains(index))
      reRootModificationTree();
    return SAFE_DEREF(node_->value(index));
  }

  /// Changes array value for specified index.
  /// Complexity: constant.
  /// \param index array index
  /// \param value new value
  /// \return array containing new value
  [[nodiscard]] PersistentArray setValue(const std::size_t index, const T &value) const {
    CONTRACT_EXPECT(index < size_);
    return modify(PersistentNode::makeChangeSet(node_, index, value), size_);
  }

  /// Changes array value for specified index.
  /// Complexity: constant.
  /// \param index array index
  /// \param value new value
  /// \return array containing new value
  [[nodiscard]] PersistentArray setValue(const std::size_t index, T &&value) const {
    CONTRACT_EXPECT(index < size_);
    return modify(PersistentNode::makeChangeSet(node_, index, std::move(value)), size_);
  }

  /// Appends value to the end of array
  /// Complexity: linear.
  /// \param value value to append
  /// \return array containing appended value
  [[nodiscard]] PersistentArray pushBack(const value_type &value) const {
    return emplaceBack(value);
  }

  /// Appends value to the end of array
  /// Complexity: linear.
  /// \param value value to append
  /// \return array containing appended value
  [[nodiscard]] PersistentArray pushBack(value_type &&value) const {
    return emplaceBack(std::move(value));
  }

  /// Constructs an element in-place at the end of array
  /// Complexity: linear.
  /// \param args arguments to create value
  /// \return array containing appended value
  template <class... Args> [[nodiscard]] PersistentArray emplaceBack(Args &&...args) const {
    auto &root = findOrCreateRoot();
    CONTRACT_ASSERT(node_);

    auto origin = node_;
    if (root.contains(size_))
      /// Should create change-set
      origin = PersistentNode::makeChangeSet(node_, size_, std::forward<Args>(args)...);
    else
      /// Should extend original array with value
      root.extend(std::forward<Args>(args)...);

    return modify(std::move(origin), size_ + 1);
  }

  /// Removes last element from array
  /// Complexity: constant.
  /// \return array without removed element
  [[nodiscard]] PersistentArray popBack() const {
    CONTRACT_EXPECT(!empty());

    /// Simply decrement size and don't remove value from original array,
    /// since we don't know is this value referenced from another array or not
    return modify(node_, size_ - 1);
  }

  /// Returns an iterator to the beginning.
  [[nodiscard]] auto begin() const { return iterator{*this}; }
  /// Returns an iterator to the end.
  [[nodiscard]] auto end() const { return iterator{*this, size()}; }
  /// Returns a reverse iterator to the beginning.
  [[nodiscard]] auto rbegin() const { return reverse_iterator{end()}; }
  /// Returns a reverse iterator to the end.
  [[nodiscard]] auto rend() const { return reverse_iterator{begin()}; }

private:
  std::size_t size_ = 0;
  mutable typename PersistentNode::Ptr node_; // lazy initialization

  explicit PersistentArray(const std::size_t size, typename PersistentNode::Ptr node = {},
                           Undo::UndoRedoManager<PersistentArray> undoRedoManager = {})
      : Base(std::move(undoRedoManager)), size_(size), node_(std::move(node)) {}

  [[nodiscard]] PersistentNode &findOrCreateRoot() const {
    if (!node_)
      return SAFE_DEREF(node_ = PersistentNode::makeRoot());

    auto foundNode = node_;
    while (foundNode && !foundNode->isRoot())
      foundNode = foundNode->parent();
    return SAFE_DEREF(foundNode);
  }

  /// Returns modified array and generates undo action.
  [[nodiscard]] PersistentArray modify(typename PersistentNode::Ptr newNode,
                                       const std::size_t newSize) const {

    const auto undo = [size = size_, node = node_](auto manager) {
      return PersistentArray{size, node, std::move(manager)};
    };
    const auto redo = [size = newSize, node = std::move(newNode)](auto manager) {
      return PersistentArray{size, node, std::move(manager)};
    };
    return redo(this->undoManager().pushUndo(Undo::createAction<PersistentArray>(undo, redo)));
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

template <class T>
[[nodiscard]] bool operator==(const PersistentArray<T> &lhs, const PersistentArray<T> &rhs) {
  if (lhs.size() != rhs.size())
    return false;
  return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <class T>
[[nodiscard]] bool operator!=(const PersistentArray<T> &lhs, const PersistentArray<T> &rhs) {
  return !(lhs == rhs);
}

/// Iterator class for @PersistentArray
template <class T> class PersistentArrayIterator final {
  friend class PersistentArray<T>;
  using PositionIndex = std::size_t;

public:
  using value_type = typename PersistentArray<T>::value_type;
  using reference = const value_type &;
  using pointer = const value_type *;
  using difference_type = std::make_signed_t<PositionIndex>;

  PersistentArrayIterator(const PersistentArrayIterator &) = default;
  PersistentArrayIterator &operator=(const PersistentArrayIterator &) = default;

  /// Access to value which iterator points to.
  [[nodiscard]] reference operator*() const { return target_.value(currentIndex_); }
  /// Access to value which iterator points to.
  [[nodiscard]] pointer operator->() const { return &target_.value(currentIndex_); }
  /// Access to element with specified offset.
  [[nodiscard]] reference operator[](const difference_type offset) const {
    return *(*this + offset);
  }

  /// Increments iterator.
  PersistentArrayIterator &operator++() { return *this += 1u; }
  /// Decrements iterator.
  PersistentArrayIterator &operator--() { return *this -= 1u; }

  PersistentArrayIterator operator++(int) {
    PersistentArrayIterator tmp = *this;
    ++*this;
    return tmp;
  }

  PersistentArrayIterator &operator--(int) {
    PersistentArrayIterator tmp = *this;
    --*this;
    return tmp;
  }

  PersistentArrayIterator &operator+=(const difference_type difference) {
    CONTRACT_EXPECT(verifyOffset(difference));
    currentIndex_ += difference;
    return *this;
  }

  PersistentArrayIterator &operator-=(const difference_type difference) {
    return *this += -difference;
  }

  [[nodiscard]] PersistentArrayIterator operator+(const difference_type difference) {
    PersistentArrayIterator tmp = *this;
    return tmp += difference;
  }

  [[nodiscard]] PersistentArrayIterator operator-(const difference_type difference) {
    PersistentArrayIterator tmp = *this;
    return tmp -= difference;
  }

  [[nodiscard]] difference_type operator-(const PersistentArrayIterator &other) const {
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
  [[nodiscard]] bool verifyOffset(const difference_type difference) const noexcept {
    if (difference > 0)
      return difference <= target_.size() - currentIndex_;
    return -difference <= currentIndex_;
  }

  [[nodiscard]] bool verifyCompatibility(const PersistentArrayIterator &other) const noexcept {
    return &target_ == &other.target_;
  }

  explicit PersistentArrayIterator(const PersistentArray<T> &target,
                                   const PositionIndex currentIndex = 0u) noexcept
      : target_(target), currentIndex_(currentIndex) {}

  const PersistentArray<T> &target_;
  PositionIndex currentIndex_;
};

} // namespace Persistence

namespace std {
/// Specialization for std::iterator_traits
template <class T> struct iterator_traits<Persistence::PersistentArrayIterator<T>> {
  using iterator_category = random_access_iterator_tag;
  using value_type = typename Persistence::PersistentArrayIterator<T>::value_type;
  using pointer = typename Persistence::PersistentArrayIterator<T>::pointer;
  using reference = typename Persistence::PersistentArrayIterator<T>::reference;
  using difference_type = typename Persistence::PersistentArrayIterator<T>::difference_type;
};
} // namespace std

#endif
