#ifndef HAMT_UTILS_H
#define HAMT_UTILS_H

#include <Common/ContractExceptions.h>
#include <Common/VectorUtils.h>

#include <bitset>
#include <memory>
#include <variant>

namespace Persistence::Detail {

using HamtBit = std::size_t;
using HamtHash = std::size_t;
template <class Traits> using Bitmap = std::bitset<Traits::Capacity>;

template <class Traits> class IHamtNode;
template <class Traits> class HamtValueNode;
template <class Traits> class HamtBitmapNode;
template <class Traits> class HamtCollisionNode;
template <class Traits> using HamtNodeSPtr = std::shared_ptr<const IHamtNode<Traits>>;
template <class Traits> using HamtNodeWPtr = std::weak_ptr<const IHamtNode<Traits>>;
template <class Traits> using HamtNodeList = std::vector<HamtNodeSPtr<Traits>>;

/// Creates bitmap with single bit
template <class Traits>
[[nodiscard]] constexpr decltype(auto) createBitmap(const HamtBit bit) noexcept {
  return Bitmap<Traits>{1ull << bit};
}

/// @class IHamtVisitor
/// Interface for HAMT nodes visitor.
template <class Traits> class IHamtVisitor {
public:
  virtual ~IHamtVisitor() = default;

  [[nodiscard]] virtual HamtHash hash() const = 0;

  [[nodiscard]] virtual HamtNodeSPtr<Traits>
  visit(const std::shared_ptr<const HamtValueNode<Traits>> &) = 0;

  [[nodiscard]] virtual HamtNodeSPtr<Traits>
  visit(const std::shared_ptr<const HamtBitmapNode<Traits>> &) = 0;

  [[nodiscard]] virtual HamtNodeSPtr<Traits>
  visit(const std::shared_ptr<const HamtCollisionNode<Traits>> &) = 0;
};

/// @class IHamtNode
/// Interface for HAMT nodes.
template <class Traits> class IHamtNode {
public:
  virtual ~IHamtNode() = default;
  [[nodiscard]] virtual HamtNodeSPtr<Traits> accept(IHamtVisitor<Traits> &visitor) const = 0;

  [[nodiscard]] virtual const HamtNodeList<Traits> &children() const noexcept = 0;
  [[nodiscard]] virtual std::size_t childrenCount() const noexcept = 0;
};

//////////////////////////////////////////////////////////////////////////
///                                                                    ///
///                     HAMT NODES IMPLEMENTATIONS                     ///
///                                                                    ///
//////////////////////////////////////////////////////////////////////////

/// @class HamtNodeBase
/// HAMT base node class.
template <class Traits> class HamtNodeBase : public IHamtNode<Traits> {
public:
  explicit HamtNodeBase(HamtNodeList<Traits> children = {}) : children_(std::move(children)) {}

  HamtNodeBase(const HamtNodeBase &) = delete;
  HamtNodeBase &operator=(const HamtNodeBase &) = delete;

  /// Returns count of children
  [[nodiscard]] const HamtNodeList<Traits> &children() const noexcept override { return children_; }
  /// Returns children
  [[nodiscard]] std::size_t childrenCount() const noexcept override { return children_.size(); }

private:
  HamtNodeWPtr<Traits> parent_;
  HamtNodeList<Traits> children_;
};

/// @class HamtValueNode
/// Implementation of node containing value.
template <typename Traits>
class HamtValueNode final : public HamtNodeBase<Traits>,
                            public std::enable_shared_from_this<HamtValueNode<Traits>> {
  using KeyType = typename Traits::KeyType;
  using ValueType = typename Traits::ValueType;
  using KeyValue = std::pair<KeyType, ValueType>;

public:
  using Ptr = std::shared_ptr<const HamtValueNode>;

  template <class... Args> [[nodiscard]] static Ptr create(Args &&...args) {
    return Ptr{new HamtValueNode(std::forward<Args>(args)...)};
  }

  /// Returns hash of key
  [[nodiscard]] HamtHash hash() const { return hash_; }
  /// Returns key
  [[nodiscard]] const KeyType &key() const { return keyValue_.first; }
  /// Returns value
  [[nodiscard]] const ValueType &value() const { return keyValue_.second; }
  /// Return key-value pair
  [[nodiscard]] const KeyValue &keyValue() const { return keyValue_; }

  /// Accepts visitor
  [[nodiscard]] HamtNodeSPtr<Traits> accept(IHamtVisitor<Traits> &visitor) const override {
    return visitor.visit(this->shared_from_this());
  }

private:
  template <class Arg, class... Args>
  explicit HamtValueNode(Arg &&arg, Args &&...args)
      : keyValue_(std::forward<Arg>(arg), std::forward<Args>(args)...),
        hash_(typename Traits::Hasher{}(keyValue_.first)) {}

  KeyValue keyValue_;
  HamtHash hash_ = 0u;
};

/// @class HamtBitmapNode
/// Node containing table of sub-nodes.
template <typename Traits>
class HamtBitmapNode final : public HamtNodeBase<Traits>,
                             public std::enable_shared_from_this<HamtBitmapNode<Traits>> {
  using Bitmap = Bitmap<Traits>;

public:
  using Ptr = std::shared_ptr<const HamtBitmapNode>;

  /// Creates node holding single getChildAtBit
  [[nodiscard]] static Ptr create(const HamtBit bit, HamtNodeSPtr<Traits> node) {
    return create(createBitmap<Traits>(bit), HamtNodeList<Traits>{std::move(node)});
  }

  /// Creates node holding specified children
  [[nodiscard]] static Ptr create(const Bitmap bitmap, HamtNodeList<Traits> children) {
    return Ptr{new HamtBitmapNode(bitmap, std::move(children))};
  }

  /// Inserts node for specified bit. Leaves original node unchanged.
  [[nodiscard]] Ptr insertBit(const HamtBit bit, HamtNodeSPtr<Traits> node) const {
    CONTRACT_EXPECT(!containsBit(bit));
    return create(bitmap_ & createBitmap<Traits>(bit),
                  Util::vectorInserted(this->children(), this->children().begin() + bitToIndex(bit),
                                       std::move(node)));
  }

  /// Replaces node for specified bit. Leaves original node unchanged.
  [[nodiscard]] Ptr replaceBit(const HamtBit bit, HamtNodeSPtr<Traits> node) const {
    CONTRACT_EXPECT(containsBit(bit));
    return create(bitmap_,
                  Util::vectorReplaced(this->children(), this->children().begin() + bitToIndex(bit),
                                       std::move(node)));
  }

  /// Erase node for specified bit. Leaves original node unchanged.
  [[nodiscard]] Ptr eraseBit(const HamtBit bit) const {
    CONTRACT_EXPECT(containsBit(bit));
    return create(bitmap_ ^ createBitmap<Traits>(bit),
                  Util::vectorErased(this->children(), this->children().begin() + bitToIndex(bit)));
  }

  /// Returns sub-node for specified bit.
  [[nodiscard]] decltype(auto) getChildAtBit(const HamtBit bit) const {
    CONTRACT_EXPECT(containsBit(bit));
    return this->children().at(bitToIndex(bit));
  }

  /// Checks is bitmap containsBit specified bit.
  [[nodiscard]] bool containsBit(const HamtBit bit) const { return bitmap_.test(bit); }

  /// Accepts visitor
  [[nodiscard]] HamtNodeSPtr<Traits> accept(IHamtVisitor<Traits> &visitor) const override {
    return visitor.visit(this->shared_from_this());
  }

private:
  explicit HamtBitmapNode(const Bitmap bitmap = {}, HamtNodeList<Traits> children = {})
      : HamtNodeBase<Traits>(std::move(children)), bitmap_(bitmap) {}

  /// Converts bit to index in children array
  [[nodiscard]] auto bitToIndex(const HamtBit bit) const {
    auto mask = 1ull << bit;
    return (bitmap_ & Bitmap{--mask}).count();
  }

  Bitmap bitmap_;
};

/// @class HamtCollisionNode
/// Class storing colliding values
template <typename Traits>
class HamtCollisionNode final : public HamtNodeBase<Traits>,
                                public std::enable_shared_from_this<HamtCollisionNode<Traits>> {
  using HamtNodeBase<Traits>::HamtNodeBase;
  using CollidingValue = typename HamtValueNode<Traits>::Ptr;

public:
  using Ptr = std::shared_ptr<const HamtCollisionNode>;

  /// Creates node holding specified children
  [[nodiscard]] static Ptr create(HamtNodeList<Traits> children) {
    return Ptr{new HamtCollisionNode(std::move(children))};
  }

  /// Extends collision node with particular value
  [[nodiscard]] Ptr addCollision(CollidingValue value) const {
    return create(Util::vectorInserted(this->children(), this->children().end(), std::move(value)));
  }

  /// Erases element with particular key from node
  [[nodiscard]] Ptr removeCollision(const typename Traits::KeyType &key) const {
    return create(Util::vectorErased(this->children(), findImpl(key)));
  }

  /// Founds element node with particular key.
  [[nodiscard]] CollidingValue findCollision(const typename Traits::KeyType &key) const {
    const auto found = findImpl(key);
    return found != this->children().cend()
               ? std::dynamic_pointer_cast<CollidingValue::element_type>(*found)
               : nullptr;
  }

  /// Accepts visitor
  [[nodiscard]] HamtNodeSPtr<Traits> accept(IHamtVisitor<Traits> &visitor) const override {
    return visitor.visit(this->shared_from_this());
  }

private:
  /// Returns iterator to found key-value node
  [[nodiscard]] auto findImpl(const typename Traits::KeyType &key) const {
    typename Traits::KeyEqual keyEq;
    return std::find_if(
        this->children().begin(), this->children().end(), [&key, keyEq](const auto &value) {
          return keyEq(std::dynamic_pointer_cast<CollidingValue::element_type>(value)->key(), key);
        });
  }
};

//////////////////////////////////////////////////////////////////////////
///                                                                    ///
///                    HAMT VISITORS IMPLEMENTATIONS                   ///
///                                                                    ///
//////////////////////////////////////////////////////////////////////////

/// @class HamtVisitorBase
/// Base class for HAMT visitors.
template <class Traits> class HamtVisitorBase : public IHamtVisitor<Traits> {
protected:
  /// Returns current level
  [[nodiscard]] auto level() const { return level_; }

  /// Increments traversal level
  void nextLevel() { ++level_; }

  /// Returns HAMT bit at current traversal level
  [[nodiscard]] HamtBit getLevelBit(const HamtHash hash) const {
    const auto shift = Traits::BitSize * level_;
    return (hash >> shift) & Traits::BitMask;
  }

private:
  std::size_t level_ = 0u;
};

/// @class InserterVisitor
/// Visitor implementation to insertBit value into HAMT.
template <class Traits> class InserterVisitor final : public HamtVisitorBase<Traits> {
public:
  explicit InserterVisitor(typename HamtValueNode<Traits>::Ptr inserted, const bool replace = true)
      : inserted_(std::move(inserted)), replace_(replace) {}

  [[nodiscard]] HamtNodeSPtr<Traits>
  visit(const typename HamtValueNode<Traits>::Ptr &node) override {
    if (typename Traits::KeyEqual keyEq; keyEq(node->key(), inserted_->key()))
      /// The same key: update or leave unchanged
      return replace_ ? inserted_ : node;

    return resolveCollision(node);
  }

  [[nodiscard]] HamtNodeSPtr<Traits>
  visit(const typename HamtBitmapNode<Traits>::Ptr &node) override {
    const auto bit = this->getLevelBit(inserted_->hash());
    if (!node->containsBit(bit))
      /// Add new node
      return node->insertBit(bit, inserted_);

    this->nextLevel();
    const auto &child = node->getChildAtBit(bit);
    if (auto newChild = child->accept(*this); child != newChild)
      return node->replaceBit(bit, std::move(newChild));

    return node;
  }

  [[nodiscard]] HamtNodeSPtr<Traits>
  visit(const typename HamtCollisionNode<Traits>::Ptr &node) override {
    auto collision = node;

    const bool found = static_cast<bool>(collision->findCollision(inserted_->key()));
    if (found && replace_) /// Replace if required
      collision = collision->removeCollision(inserted_->key());

    return !found || replace_ ? collision->addCollision(inserted_) : collision;
  }

  [[nodiscard]] HamtHash hash() const override { return inserted_->hash(); }

private:
  [[nodiscard]] HamtNodeSPtr<Traits>
  resolveCollision(const typename HamtValueNode<Traits>::Ptr &node) {
    if (this->level() > Traits::MaxDepth)
      return HamtCollisionNode<Traits>::create({node, inserted_});

    const auto bitCurrent = this->getLevelBit(node->hash());
    const auto bitInserted = this->getLevelBit(inserted_->hash());

    /// Still collided
    if (bitCurrent == bitInserted) {
      const auto indexNode = HamtBitmapNode<Traits>::create(bitCurrent, node);
      return indexNode->accept(*this);
    }
    return HamtBitmapNode<Traits>::create(
        createBitmap<Traits>(bitCurrent) | createBitmap<Traits>(bitInserted), {node, inserted_});
  }

  typename HamtValueNode<Traits>::Ptr inserted_;
  bool replace_;
};

/// @class EraserVisitor
/// Visitor implementation to removeCollision value from HAMT.
template <class HamtTraits> class EraserVisitor final : public HamtVisitorBase<HamtTraits> {
public:
  template <class... Args>
  explicit EraserVisitor(Args &&...args)
      : key_(std::forward<Args>(args)...), hash_(typename HamtTraits::Hasher{}(key_)) {}

  [[nodiscard]] HamtNodeSPtr<HamtTraits>
  visit(const typename HamtValueNode<HamtTraits>::Ptr &node) override {
    if (typename HamtTraits::KeyEqual keyEq; keyEq(node->key(), key_))
      return nullptr; /// Found node and erased it

    return node; /// Other node found
  }

  [[nodiscard]] HamtNodeSPtr<HamtTraits>
  visit(const typename HamtBitmapNode<HamtTraits>::Ptr &node) override {
    const auto bit = this->getLevelBit(hash_);
    if (!node->containsBit(bit))
      return node; /// Key not found

    this->nextLevel();
    const auto &child = node->getChildAtBit(bit);

    if (const auto newChild = child->accept(*this); child != newChild) {
      if (!newChild) {
        /// Convert bitmap node into key-value node if it size equal to 1
        const auto nodeErased = node->eraseBit(bit);
        return nodeErased->childrenCount() > 1 ? nodeErased : nodeErased->children().at(0);
      }
      /// Removed in subtrie
      return node->replaceBit(bit, std::move(newChild));
    }
    return node; /// Key not found in subtrie
  }

  [[nodiscard]] HamtNodeSPtr<HamtTraits>
  visit(const typename HamtCollisionNode<HamtTraits>::Ptr &node) override {
    if (node->findCollision(key_)) {
      /// Convert collision node into key-value node if it size equal to 1
      const auto removed = node->removeCollision(key_);
      if (removed->childrenCount() > 1)
        return removed;
      return removed->children().at(0);
    }
    return node; /// Key not found
  }

  [[nodiscard]] HamtHash hash() const override { return hash_; }

private:
  typename HamtTraits::KeyType key_;
  HamtHash hash_ = 0u;
};

/// @class SearcherVisitor
/// Visitor implementation to search value in HAMT.
template <class Traits> class SearcherVisitor final : public HamtVisitorBase<Traits> {

public:
  template <class... Args>
  explicit SearcherVisitor(Args &&...args)
      : key_(std::forward<Args>(args)...), hash_(typename Traits::Hasher{}(key_)) {}

  [[nodiscard]] HamtNodeSPtr<Traits>
  visit(const typename HamtBitmapNode<Traits>::Ptr &node) override {
    const auto bit = this->getLevelBit(hash_);
    if (!node->containsBit(bit))
      return nullptr;
    this->nextLevel();
    return node->getChildAtBit(bit)->accept(*this);
  }

  [[nodiscard]] HamtNodeSPtr<Traits>
  visit(const typename HamtCollisionNode<Traits>::Ptr &node) override {
    return node->findCollision(key_);
  }

  [[nodiscard]] HamtNodeSPtr<Traits>
  visit(const typename HamtValueNode<Traits>::Ptr &node) override {
    return typename Traits::KeyEqual{}(node->key(), key_) ? node : nullptr;
  }

  [[nodiscard]] HamtHash hash() const override { return hash_; }

private:
  typename Traits::KeyType key_;
  HamtHash hash_ = 0u;
};

} // namespace Persistence::Detail

#endif
