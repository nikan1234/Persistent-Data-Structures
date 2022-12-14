#ifndef PERSISTENT_LIST_H
#define PERSISTENT_LIST_H

#include "UndoablePersistentCollection.h"
#include <Undo/UndoRedoManager.h>

#include <vector>
#include <list>
#include <algorithm>
#include <memory>
#include <map>
#include <cmath>

namespace Persistence {
/// simple structure that defines versioned order (for full persistence)
struct PersistenceListOrder {
  /// weights between [-weight_border; weight_border]
  const double weight_border = 2000000000000;
  /// list with versions and opposite version
  std::list<int> list;
  /// handles for list
  std::vector<std::list<int>::iterator> handles;
  /// weights for versions
  std::vector<double> weight_true;
  /// weights for opposite versions
  std::vector<double> weight_reverse;
  PersistenceListOrder(){};

  /// add version and version's opposite
  /// @param parent - version-parent for new version. The argument is ignored on the first call
  /// @return new version's number
  int add(
      int parent) {
    if (list.size() == 0) {
      handles.push_back(list.insert(list.end(), 1));
      handles.push_back(handles[0]);
      list.insert(list.end(), -1);
      weight_true.push_back(-weight_border);
      weight_true.push_back(-weight_border);
      weight_reverse.push_back(weight_border);
      weight_reverse.push_back(weight_border);
      return 1;
    }
    CONTRACT_EXPECT(parent <= handles.size());
    auto next_parent_handle = handles[parent];
    ++next_parent_handle;
    double parent_value = weight_true[parent];
    double next_parent_value;
    int next_parent = *next_parent_handle;
    if (next_parent > 0) {
      next_parent_value = weight_true[next_parent];
    } else {
      next_parent_value = weight_reverse[-next_parent];
    }
    int new_version = handles.size();
    auto version_handle = list.insert(next_parent_handle, new_version);
    handles.push_back(version_handle);
    ++version_handle;
    list.insert(version_handle, -new_version);
    double true_weight = parent_value + (next_parent_value - parent_value) / 3;
    double true_reverse = parent_value + 2 * (next_parent_value - parent_value) / 3;
    weight_true.push_back(true_weight);
    weight_reverse.push_back(true_reverse);
    if (true_weight == true_reverse) {
      double step = weight_border / weight_true.size();
      double cur = -weight_border;
      for (auto i : list) {
        if (i < 0) {
          weight_reverse[-i] = cur;
        } else {
          weight_true[i] = cur;
        }
        cur += step;
      }
    }
    return new_version;
  }

  /// compare two versions
  /// @param l - version to compare
  /// @param r - version to compare
  /// @return true, if l before r, also false
  bool less(int l, int r) { 
      CONTRACT_EXPECT(abs(l) < weight_true.size());
      CONTRACT_EXPECT(abs(r) < weight_true.size());
      double weight_l, weight_r;
      if (l < 0) {
        weight_l = weight_reverse[-l];
      } else {
        weight_l = weight_true[l];
      }
      if (r < 0) {
        weight_r = weight_reverse[-r];
      } else {
        weight_r = weight_true[r];
      }
      return weight_l < weight_r;
  }
};

/// comparator to compare versions for full persistence
struct PersistenceCmpByListVersion {
  /// structure for versioned order
  std::shared_ptr<PersistenceListOrder> listOrder_;
  /// @param listOrder - structure for versioned order
  PersistenceCmpByListVersion(std::shared_ptr<PersistenceListOrder> listOrder) : listOrder_(listOrder) {}

  /// compare two versions
  /// @param a - version to compare
  /// @param b - version to compare
  /// @return true, if a before b, also false
  bool operator()(const int a, const int b) const { 
      return listOrder_ -> less(a, b); }
};

/// limited size of the list node
template <class T> struct PersistenceListNode {
  /// list node limit
  static const int MAX_SIZE_FAT_NODE = 10;
  /// next nodes for different versions
  std::map<int, std::shared_ptr<PersistenceListNode>, PersistenceCmpByListVersion> next_;
  /// previous nodes for different versions
  std::map<int, std::shared_ptr<PersistenceListNode>, PersistenceCmpByListVersion> last_;
  /// values for different versions
  std::map<int, T, PersistenceCmpByListVersion> value_;

  /// @param version - the version with which this node exists
  /// @param value   - the value of this node for version
  /// @param last    - previous node for version
  /// @param next    - next node for version
  /// @param cmp     - comparator that compares versions
  PersistenceListNode(int version, const T& value, std::shared_ptr<PersistenceListNode> last, std::shared_ptr<PersistenceListNode> next,
           PersistenceCmpByListVersion cmp)
      : next_(std::map<int, std::shared_ptr<PersistenceListNode>, PersistenceCmpByListVersion>(cmp)),
        last_(std::map<int, std::shared_ptr<PersistenceListNode>, PersistenceCmpByListVersion>(cmp)),
        value_(std::map<int, T, PersistenceCmpByListVersion>(cmp))
  {
    next_[version] = next;
    last_[version] = last;
    value_[version] = value;
  }

  /// constructor for head_ and tail_ - special nodes, connecting version's nodes
  /// @param version - the version with which this node exists
  /// @param last    - previous node for version
  /// @param next    - next node for version
  /// @param cmp     - comparator that compares versions
  PersistenceListNode(int version, std::shared_ptr<PersistenceListNode> last,
           std::shared_ptr<PersistenceListNode> next, PersistenceCmpByListVersion cmp)
      : next_(std::map<int, std::shared_ptr<PersistenceListNode>, PersistenceCmpByListVersion>(cmp)),
        last_(std::map<int, std::shared_ptr<PersistenceListNode>, PersistenceCmpByListVersion>(cmp)),
        value_(std::map<int, T, PersistenceCmpByListVersion>(cmp)) {
    next_[version] = next;
    last_[version] = last;
  }

  bool operator==(const PersistenceListNode &other) const {
    return next_ == other.next_ && last_ == other.last_ && value_ == other.value_;
  }

  bool operator!=(const PersistenceListNode &other) const {
    return next_ != other.next_ || last_ != other.last_ || value_ != other.value_;
  }

  /// adds a value to the node if there is space in it
  /// @param version - the version for which add the value
  /// @param value   - the value to add
  /// @return true if the added, false if failed
  bool add(int version, const T& value) {
    if (value_.size() >= MAX_SIZE_FAT_NODE) {
      return false;
    }
    value_[version] = value;
    return true;
  }

  /// returns true if you can add the following node, false otherwise
  bool canSetNext() { return value_.size() == 0 || next_.size() < MAX_SIZE_FAT_NODE; }

  /// returns true if you can add a previous node, false otherwise
  bool canSetLast() { return value_.size() == 0 || last_.size() < MAX_SIZE_FAT_NODE; }

  /// add following node
  /// @param version - version for which we add the following node
  /// @param next    - the node to be added
  /// @return true if the added, false if failed
  bool setNext(int version, std::shared_ptr<PersistenceListNode> next) { 
      if (!canSetNext() && next_.find(version) == next_.end()) {
        return false;
      }
      next_[version] = next; 
      return true;
  }

  /// add previous node
  /// @param version - the version for which we add the previous node
  /// @param last    - the node to be added
  /// @return true if the added, false if failed
  bool setLast(int version, std::shared_ptr<PersistenceListNode> last) { 
      if (!canSetLast() && last_.find(version) == last_.end()) {
        return false;
      }
      last_[version] = last;
      return true;
  }

  /// copy from the src node all the following nodes after version
  /// @param src     - the node from which we copy
  /// @param version - the version from which it copies
  void copyNextAfter(std::shared_ptr<PersistenceListNode> src, int version) { 
      for (auto i = src->next_.lower_bound(version); i != src->next_.end(); ++i) {
        this->next_[i->first] = src->next_[i->first];
      }
  }

  /// copy from src node, all previous nodes after version
  /// @param src     - the node from which we copy
  /// @param version - the version from which it copies
   void copyLastAfter(std::shared_ptr<PersistenceListNode> src, int version) {
     for (auto i = src->last_.lower_bound(version); i != src->last_.end(); ++i) {
      this->last_[i->first] = src->last_[i->first];
     }
  }

  /// @param version - version
  /// @return a value for version
  T find(int version) const {
    CONTRACT_EXPECT(!value_.empty());
    auto it = value_.upper_bound(version);
    --it;
    return it -> second;
  }

  /// @param version - version
  /// @return the following node for the version
  std::shared_ptr<PersistenceListNode> getNext(int version) const {
    CONTRACT_EXPECT(!next_.empty());
    auto it = next_.upper_bound(version);
    --it;
    return it->second;
  }

  /// @param version - version
  /// @return the previous node for the version version
  std::shared_ptr<PersistenceListNode> getLast(int version) const {
    CONTRACT_EXPECT(!last_.empty());
    auto it = last_.upper_bound(version);
    --it;
    return it->second;
  }
};

/// iterator for the list
template <class T> class PersistenceListIterator final { 
  int version_;
  std::shared_ptr<PersistenceListNode<T>> node_;

  public:
  /// @param version - version
  /// @param node    - the node pointed to by the iterator
  PersistenceListIterator(int version, const std::shared_ptr<PersistenceListNode<T>> &node)
      : version_(version), node_(node) {}

  T operator*() const { return node_->find(version_); }
  PersistenceListNode<T> operator->() const { return node_; }

  PersistenceListIterator &operator++() {
    node_ = node_->getNext(version_);
    return *this;
  }

  PersistenceListIterator& operator--() {
    node_ = node_->getLast(version_);
    return *this;
  }

  PersistenceListIterator operator++(int) {
    PersistenceListIterator tmp = *this;
    node_ = node_->getNext(version_);
    return tmp;
  }

  PersistenceListIterator &operator--(int) {
    PersistenceListIterator tmp = *this;
    node_ = node_->getLast(version_);
    return tmp;
  }

  bool operator==(const PersistenceListIterator &other) const {
    return version_ == other.version_ && node_ == other.node_;
  }

  bool operator!=(const PersistenceListIterator &other) const {
    return version_ != other.version_ || node_ != other.node_;
  }
};

/// reverse iterator for list
template <class T> class PersistenceListReverseIterator final {
  int version_;
  std::shared_ptr<PersistenceListNode<T>> node_;

public:
  /// @param version - the version for which the list is
  /// @param node    - the node pointed to by the iterator
  PersistenceListReverseIterator(int version, const std::shared_ptr<PersistenceListNode<T>> &node)
      : version_(version), node_(node) {}

  T operator*() const { return node_->find(version_); }
  PersistenceListNode<T> operator->() const { return node_; }

  PersistenceListReverseIterator &operator--() {
    node_ = node_->getNext(version_);
    return *this;
  }

  PersistenceListReverseIterator &operator++() {
    node_ = node_->getLast(version_);
    return *this;
  }

  PersistenceListReverseIterator operator--(int) {
    node_ = node_->getNext(version_);
    return *this;
  }

  PersistenceListReverseIterator &operator++(int) {
    node_ = node_->getLast(version_);
    return *this;
  }

  bool operator==(const PersistenceListReverseIterator &other) const {
    return version_ == other.version_ && node_ == other.node_;
  }

  bool operator!=(const PersistenceListReverseIterator &other) const {
    return version_ != other.version_ || node_ != other.node_;
  }
};

/// version of persistent list
/// @tparam T - type of element
template <class T>
class PersistentList final : public UndoablePersistentCollection<PersistentList<T>> {
private:
  /// version
  int version_;
  std::shared_ptr<PersistenceListOrder> listOrder_;
  /// special node without value
  std::shared_ptr<PersistenceListNode <T>> head_;
  /// special node without value
  std::shared_ptr<PersistenceListNode <T>> tail_;
  /// size
  size_t size_;

  /// @param version   - version
  /// @param listOrder - structure for version comparison
  /// @param head      - node that holds the first nodes in all versions
  /// @param tail      - node that keeps the latest nodes in all versions
  /// @param size      - length of this version of the list
  PersistentList(int version, std::shared_ptr<PersistenceListOrder> listOrder, std::shared_ptr<PersistenceListNode <T>> head, std::shared_ptr<PersistenceListNode<T>> tail, size_t size,
                 Undo::UndoRedoManager<PersistentList> undoRedoManager)
      : UndoablePersistentCollection<PersistentList<T>>(std::move(undoRedoManager)), version_(version), listOrder_(listOrder),
        head_(head), tail_(tail), size_(size) {
  }
  
  /// @param new_version  - version for new list
  /// @param size         - size for new list
  /// @return list with version_ new_version, size_ size and current head_ and tail_
  PersistentList<T> getChildren(int new_version, size_t size) const {
    const auto undo = [version = version_, listOrder = listOrder_, head = head_, tail = tail_, size_undo = size_](auto manager) {
      return PersistentList{version, listOrder, head, tail, size_undo, std::move(manager)};
    };

    const auto redo = [version = new_version, listOrder = listOrder_, head = head_, tail = tail_, size_undo = size](auto manager) {
      return PersistentList{version, listOrder, head, tail, size_undo, std::move(manager)};
    };
    Undo::UndoRedoManager<PersistentList> newUndoRedoManager =
        undoManager().pushUndo(Undo::createAction<PersistentList>(undo, redo));
    return PersistentList<T>(new_version, listOrder_, head_, tail_, size, std::move(newUndoRedoManager));
  }

  /// @param version - version
  /// @param index   - index
  /// @return node with index number in version list
  std::shared_ptr<PersistenceListNode <T>> findNodeByIndex(int version, int index) const {
    if (index >= size_) {
      throw std::exception("index more size");
    }
    std::shared_ptr<PersistenceListNode <T>> ptr = head_;
    ++index; //?????? ??? head_ ??? ????????
    for (int i = 0; i < index; ++i) {
      if (ptr == nullptr) {
        throw std::exception("not value by index for current version");
      }
      ptr = ptr->getNext(version);
    }
    if (ptr == nullptr) {
      throw std::exception("not value by index for current version");
    }
    return ptr;
  }

  /// @param index   - index
  /// @return the node with index index in the current list
  std::shared_ptr<PersistenceListNode <T>> findNodeByIndex(int index) const {
    return findNodeByIndex(version_, index);
  }

  /// makes a new node when the old one gets too fat
  /// @param version - version with which the new node is added
  /// @param value   - value that this node will have on version version
  /// @param last    - node to which we insert
  /// @param next    - node after which we insert
  void makeNewNode(int version, const T &value, std::shared_ptr<PersistenceListNode <T>> last,
                   std::shared_ptr<PersistenceListNode <T>> next) {
    std::shared_ptr<PersistenceListNode <T>> new_node =
        std::make_shared<PersistenceListNode <T>>(version, value, nullptr, nullptr, PersistenceCmpByListVersion(listOrder_));
    auto cur_last = last;
    auto cur_next = new_node;
    while (!cur_last->canSetNext()) {
      std::shared_ptr<PersistenceListNode <T>> cur_new_node = std::make_shared<PersistenceListNode <T>>(version, cur_last->find(version), 
          cur_last->getLast(version), cur_next, PersistenceCmpByListVersion(listOrder_));
      cur_new_node->copyNextAfter(cur_last, version);
      cur_last->getLast(version)->setNext(version, cur_new_node);
      cur_next->setLast(version, cur_new_node);
      cur_next = cur_new_node;
      cur_last = cur_last->getLast(version);
    }
    cur_last->setNext(version, cur_next);
    cur_next->setLast(version, cur_last);
    cur_next = next;
    cur_last = new_node;
    while (!cur_next->canSetLast()) {
      std::shared_ptr<PersistenceListNode <T>> cur_new_node =
          std::make_shared<PersistenceListNode <T>>(version, cur_next->find(version), cur_last,
              cur_next->getNext(version), PersistenceCmpByListVersion(listOrder_));
      cur_new_node->copyLastAfter(cur_next, version);
      cur_next->getNext(version)->setLast(version, cur_new_node);
      cur_last->setNext(version, cur_new_node);
      cur_last = cur_new_node;
      cur_next = cur_next->getNext(version);
    }
    cur_last->setNext(version, cur_next);
    cur_next->setLast(version, cur_last);
  }

  /// delete the node for some version while maintaining the restriction on the "thickness"
  /// @param node_version - version from which the node is deleted 
  /// @param old_version  - version before node_version
  /// @param new_node     - the node to be deleted
  void dropNode(int version, int old_version, const std::shared_ptr<PersistenceListNode <T>>& new_node) {
    auto cur_last = new_node->getLast(old_version);
    auto cur_next = new_node->getNext(old_version);
    while (!cur_last->canSetNext()) {
      std::shared_ptr<PersistenceListNode <T>> cur_new_node = std::make_shared<PersistenceListNode <T>>(
          version, cur_last->find(old_version), cur_last->getLast(old_version),
                                     cur_next, PersistenceCmpByListVersion(listOrder_));
      cur_new_node->copyNextAfter(cur_last, version);
      cur_last->getLast(old_version)->setNext(version, cur_new_node);
      cur_next->setLast(version, cur_new_node);
      cur_next = cur_new_node;
      cur_last = cur_last->getLast(old_version);
    }
    cur_last->setNext(version, cur_next);
    cur_next->setLast(version, cur_last);

    cur_last = new_node->getLast(old_version);
    cur_next = new_node->getNext(old_version);
    while (!cur_next->canSetLast()) {
      std::shared_ptr<PersistenceListNode <T>> cur_new_node =
          std::make_shared<PersistenceListNode <T>>(version, cur_next->find(old_version), cur_last,
                                     cur_next->getNext(old_version), PersistenceCmpByListVersion(listOrder_));
      cur_new_node->copyLastAfter(cur_next, version);
      cur_next->getNext(old_version)->setLast(version, cur_new_node);
      cur_last->setNext(version, cur_new_node);
      cur_last = cur_new_node;
      cur_next = cur_next->getNext(old_version);
    }
    cur_last->setNext(version, cur_next);
    cur_next->setLast(version, cur_last);
  }

public:
  PersistentList() : version_(1) { 
    listOrder_ = std::make_shared<PersistenceListOrder>();
    listOrder_ -> add(0);
    head_ = std::make_shared<PersistenceListNode <T>>(version_, nullptr, nullptr, PersistenceCmpByListVersion(listOrder_));
    tail_ = std::make_shared<PersistenceListNode <T>>(version_, head_, nullptr, PersistenceCmpByListVersion(listOrder_));
    head_->setNext(1, tail_);
    size_ = 0;
  }

  /// create a list based on a list
  PersistentList(const std::initializer_list<T>& v) : version_(1) {
    listOrder_ = std::make_shared<PersistenceListOrder>();
    listOrder_->add(0);
    head_ = std::make_shared<PersistenceListNode <T>>(version_, nullptr, nullptr, PersistenceCmpByListVersion(listOrder_));
    std::shared_ptr<PersistenceListNode <T>> ptr = head_;
    for (auto i : v) {
      auto fatNode = std::make_shared<PersistenceListNode <T>>(version_, i, ptr, nullptr, PersistenceCmpByListVersion(listOrder_));
      ptr->setNext(version_, fatNode);
      fatNode->setLast(version_, ptr);
      ptr = fatNode;
    }
    tail_ = std::make_shared<PersistenceListNode <T>>(version_, ptr, nullptr, PersistenceCmpByListVersion(listOrder_));
    ptr->setNext(version_, tail_);
    size_ = v.size();
  }

  /// find value by index
  /// @param index - index
  T find(int index) const { 
      std::shared_ptr<PersistenceListNode <T>> ptr = findNodeByIndex(index);
      return ptr->find(version_);
  }

  /// set value by index
  /// @param index - index
  /// @param value - value
  PersistentList set(int index, const T& value) { 
    std::shared_ptr<PersistenceListNode <T>> ptr = findNodeByIndex(index);
    int new_version = listOrder_ -> add(version_);
    if (!ptr->add(new_version, value)) {
      makeNewNode(new_version, value, ptr->getLast(version_), ptr->getNext(version_));
    }
    if (!ptr->add(-new_version, ptr->find(version_))) {
      makeNewNode(-new_version, ptr->find(version_), ptr->getLast(version_), ptr->getNext(version_));
    }
    return getChildren(new_version, size_); 
  }

  /// remove an element from the list
  /// @param index - index of the element to remove
  PersistentList erase(int index) { 
    std::shared_ptr<PersistenceListNode <T>> ptr = findNodeByIndex(index);
    auto last = ptr->getLast(version_);
    auto next = ptr->getNext(version_);
    int new_version = listOrder_->add(version_);
    dropNode(new_version, version_, ptr);
    auto ptr1 = findNodeByIndex(version_, index);
    makeNewNode(-new_version, ptr1->find(version_), last, next);
    return getChildren(new_version, size_ - 1); 
  }

  /// add element before index
  /// @param index - index before
  /// @param value - value
  PersistentList insert(int index, const T& value) { 
     int new_version = listOrder_->add(version_);
     std::shared_ptr<PersistenceListNode <T>> ptr = findNodeByIndex(index);
     auto last = ptr->getLast(version_);
     makeNewNode(new_version, value, last, ptr);
     auto ptr1 = findNodeByIndex(new_version, index);
     dropNode(-new_version, new_version, ptr1);
     return getChildren(new_version, size_ + 1); 
  }

  /// add value to head
  /// @param value - value to add
  PersistentList pushFront(const T &value) {
      return insert(0, value);
  }

  /// add value to tail
  /// @param value - value to add
  PersistentList pushBack(const T &value) {
    int new_version = listOrder_->add(version_);
    auto last = tail_->getLast(version_);
    auto next = tail_;
    makeNewNode(new_version, value, last, next);
    auto ptr1 = tail_->getLast(new_version);
    dropNode(-new_version, new_version, ptr1);
    return getChildren(new_version, size_ + 1);
  }

  /// remove from head
  PersistentList popFront() { return erase(0); }

  /// remove from tail
  PersistentList popBack() { return erase(size_ - 1); }

  PersistenceListIterator<T> begin() const { return PersistenceListIterator<T>(version_, head_->getNext(version_)); }

  PersistenceListIterator<T> end() const { return PersistenceListIterator<T>(version_, tail_); }

  PersistenceListReverseIterator<T> rbegin() const {
    return PersistenceListReverseIterator<T>(version_, tail_->getLast(version_));
  }

  PersistenceListReverseIterator<T> rend() const { return PersistenceListReverseIterator<T>(version_, head_); }

  /// list size
  size_t size() const { return size_;}

};
}
#endif
