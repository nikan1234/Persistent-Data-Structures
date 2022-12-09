#ifndef PERSISTENT_DICT_H
#define PERSISTENT_DICT_H

#include "UndoablePersistentCollection.h"

#include <Collections/HamtUtils.h>
#include <Common/ContractExceptions.h>

#include <memory>
#include <optional>

namespace Persistence {

/// @class PersistentHashMap
/// A persistent rendition of Hash Array Mapped Trie. Uses path copying for persistence.
template <class Key, class Value, class Hash = std::hash<Key>, class KeyEq = std::equal_to<Key>>
class PersistentHashMap final
    : public UndoablePersistentCollection<PersistentHashMap<Key, Value, Hash, KeyEq>> {

  using Base = UndoablePersistentCollection<PersistentHashMap>;

  /// Settings for HAMT storage
  struct HamtTraits {
    using KeyType = const Key;
    using ValueType = Value;
    using KeyEqual = KeyEq;
    using Hasher = Hash;
    static constexpr std::size_t BitSize = 5u;
    static constexpr std::size_t BitMask = 0x1F;
    static constexpr std::size_t Capacity = 32;
    static constexpr std::size_t MaxDepth = sizeof(Detail::HamtHash) * 8u / BitSize - 1u;
  };

  class PersistentHashMapIterator;

public:
  using value_type = std::pair<const Key, Value>;
  using iterator = PersistentHashMapIterator;

  PersistentHashMap() = default;

  PersistentHashMap(std::initializer_list<value_type> values) {
    for (const auto &value : values) {
      *this = this->insert(value, false);
      this->undoManager() = {}; // clear undo
    }
  }

  /// Constructs map from other
  PersistentHashMap(PersistentHashMap &&other) noexcept
      : PersistentHashMap(other.size_, std::move(other.hamtRoot_), std::move(other.undoManager())) {
    other.size_ = 0u;
  }

  /// Moves map from other to this
  PersistentHashMap &operator=(PersistentHashMap &&other) noexcept {
    if (this == &other)
      return *this;

    hamtRoot_ = std::move(other.hamtRoot_);
    size_ = other.size_;
    other.size_ = 0;
    static_cast<Base &>(*this) = std::move(other);
    return *this;
  }

  /// Returns size of map
  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  /// Returns true if map is empty
  [[nodiscard]] bool empty() const noexcept { return size_ == 0u; }

  /// Inserts new node into map
  /// \param keyValue key-value to insert
  /// \param replace if true then replaces previously stored values
  [[nodiscard]] PersistentHashMap insert(const value_type &keyValue,
                                         const bool replace = true) const {
    auto node = Detail::HamtValueNode<HamtTraits>::create(keyValue);
    if (!hamtRoot_)
      return modifyHamt(std::move(node), 1u);

    Detail::InserterVisitor<HamtTraits> inserter{std::move(node), replace};
    auto [status, newRoot] = hamtRoot_->accept(inserter);
    return modifyHamt(std::move(newRoot),
                      status == Detail::HamtVisitorStatus::Resized ? size_ + 1 : size_);
  }

  /// Erases key from map. Does nothing if key absent.
  /// \param key key to erase
  [[nodiscard]] PersistentHashMap erase(const Key &key) const {
    if (!hamtRoot_)
      return modifyHamt(nullptr, 0u);

    Detail::EraserVisitor<HamtTraits> eraser{key};
    auto [status, newRoot] = hamtRoot_->accept(eraser);
    return modifyHamt(std::move(newRoot),
                      status == Detail::HamtVisitorStatus::Resized ? size_ - 1 : size_);
  }

  /// Finds value for particular key
  /// \param key to search
  [[nodiscard]] std::optional<std::reference_wrapper<const Value>> find(const Key &key) const {
    Detail::SearcherVisitor<HamtTraits> searcher{key};
    if (!hamtRoot_)
      return std::nullopt;

    const auto [_, found] = hamtRoot_->accept(searcher);
    if (auto value = std::dynamic_pointer_cast<const Detail::HamtValueNode<HamtTraits>>(found))
      return value->value();

    return std::nullopt;
  }

  /// Returns forward iterator to begin
  [[nodiscard]] auto begin() const { return PersistentHashMapIterator(hamtRoot_); }
  /// Returns forward iterator to end
  [[nodiscard]] auto end() const { return PersistentHashMapIterator(); }

  /// Checks is key present in map
  /// \param key to check
  [[nodiscard]] bool contains(const Key &key) const { return find(key) != std::nullopt; }

private:
  using HamtRoot = Detail::HamtNodeSPtr<HamtTraits>;
  using UndoManager = Undo::UndoRedoManager<PersistentHashMap>;

  std::size_t size_ = 0u;
  HamtRoot hamtRoot_;

  explicit PersistentHashMap(const std::size_t size, HamtRoot hamtRoot,
                             UndoManager undoRedoManager = {})
      : Base(std::move(undoRedoManager)), size_(size), hamtRoot_(std::move(hamtRoot)) {}

  /// Returns modified map and generates undo action.
  [[nodiscard]] PersistentHashMap modifyHamt(HamtRoot newRoot, const std::size_t newSize) const {
    const auto undo = [size = size_, root = hamtRoot_](auto manager) {
      return PersistentHashMap{size, root, std::move(manager)};
    };
    const auto redo = [size = newSize, root = std::move(newRoot)](auto manager) {
      return PersistentHashMap{size, root, std::move(manager)};
    };
    return redo(this->undoManager().pushUndo(Undo::createAction<PersistentHashMap>(undo, redo)));
  }

  /// @class PersistentHashMapIterator
  /// Implementation of iterator
  class PersistentHashMapIterator {
  public:
    using value_type = PersistentHashMap::value_type;
    using reference = const value_type &;
    using pointer = const value_type *;
    using difference_type = std::make_signed_t<std::size_t>;

    /// Creates iterator from tree node and founds first value
    explicit PersistentHashMapIterator(Detail::HamtNodeSPtr<HamtTraits> node = {})
        : iteration_(node ? std::make_shared<IterationEntry>(node) : nullptr) {
      while (iteration_ && !getValue())
        traverseNext();
    }

    [[nodiscard]] reference operator*() const { return getValue()->keyValue(); }
    [[nodiscard]] pointer operator->() const { return &**this; }

    PersistentHashMapIterator &operator++() {
      do {
        traverseNext();
      } while (iteration_ && !getValue());
      return *this;
    }

    PersistentHashMapIterator operator++(int) {
      const auto copy = *this;
      ++(*this);
      return copy;
    }

    [[nodiscard]] bool operator==(const PersistentHashMapIterator &other) const {
      return iteration_ == other.iteration_;
    }

    [[nodiscard]] bool operator!=(const PersistentHashMapIterator &other) const {
      return !(*this == other);
    }

  private:
    [[nodiscard]] auto getValue() const {
      return std::dynamic_pointer_cast<const Detail::HamtValueNode<HamtTraits>>(
          iteration_->node.lock());
    }

    void traverseNext() {
      const auto prevIteration = iteration_;
      iteration_ = iteration_->next;
      for (const auto &child : prevIteration->node.lock()->children())
        iteration_ = std::make_shared<IterationEntry>(child, iteration_);
    }

    struct IterationEntry {
      explicit IterationEntry(Detail::HamtNodeWPtr<HamtTraits> node,
                              std::shared_ptr<IterationEntry> next = {})
          : node(std::move(node)), next(std::move(next)) {}

      Detail::HamtNodeWPtr<HamtTraits> node;
      std::shared_ptr<IterationEntry> next;
    };
    std::shared_ptr<IterationEntry> iteration_;
  };
};

template <class Key, class Value, class Hash, class KeyEq>
[[nodiscard]] bool operator==(const PersistentHashMap<Key, Value, Hash, KeyEq> &lhs,
                              const PersistentHashMap<Key, Value, Hash, KeyEq> &rhs) {
  if (lhs.size() != rhs.size())
    return false;

  for (const auto &lhsValue : lhs)
    if (const auto rhsValue = rhs.find(lhsValue.first);
        !(rhsValue && rhsValue->get() == lhsValue.second))
      return false;
  return true;
}

template <class Key, class Value, class Hash, class KeyEq>
[[nodiscard]] bool operator!=(const PersistentHashMap<Key, Value, Hash, KeyEq> &lhs,
                              const PersistentHashMap<Key, Value, Hash, KeyEq> &rhs) {
  return !(lhs == rhs);
}

} // namespace Persistence

namespace std {
/// Specialization for std::iterator_traits
template <class Key, class Value, class Hash, class KeyEq>
struct iterator_traits<Persistence::PersistentHashMap<Key, Value, Hash, KeyEq>> {
private:
  using Iterator = typename Persistence::PersistentHashMap<Key, Value, Hash, KeyEq>::Iterator;

public:
  using iterator_category = forward_iterator_tag;
  using value_type = typename Iterator::value_type;
  using pointer = typename Iterator::pointer;
  using reference = typename Iterator::reference;
  using difference_type = typename Iterator::difference_type;
};
} // namespace std

#endif
