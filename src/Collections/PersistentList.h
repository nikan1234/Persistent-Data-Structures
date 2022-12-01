#ifndef PERSISTENT_LIST_H
#define PERSISTENT_LIST_H

#include "UndoablePersistentCollection.h"
#include <Undo/UndoRedoManager.h>

#include <vector>
#include <list>
#include <algorithm>
#include <memory>
#include <cassert>
#include <map>
#include <cmath>

namespace Persistence {
/// simple structure that defines versioned order (for full persistence)
struct ListOrder {
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
  ListOrder(){};

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
    assert(parent <= handles.size(), "version is not exist");
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
      assert(abs(l) < weight_true.size(), "ListOrder, l < weidht_true.size()");
      assert(abs(r) < weight_true.size(), "ListOrder, r < weidht_reverse.size()");
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
struct CmpByListVersion {
  /// structure for versioned order
  std::shared_ptr<ListOrder> listOrder_;
  /// @param listOrder - structure for versioned order
  CmpByListVersion(std::shared_ptr<ListOrder> listOrder) : listOrder_(listOrder) {}

  /// compare two versions
  /// @param a - version to compare
  /// @param b - version to compare
  /// @return true, if a before b, also false
  bool operator()(const int a, const int b) const { 
      return listOrder_ -> less(a, b); }
};

/// limited size of the list node
template <class T> struct ListNode {
  /// list node limit
  static const int MAX_SIZE_FAT_NODE = 10;
  /// next nodes for different versions
  std::map<int, std::shared_ptr<ListNode>, CmpByListVersion> next_;
  /// previous nodes for different versions
  std::map<int, std::shared_ptr<ListNode>, CmpByListVersion> last_;
  /// values for different versions
  std::map<int, T, CmpByListVersion> value_;

  /// @param version - the version with which this node exists
  /// @param value   - the value of this node for version
  /// @param last    - previous node for version
  /// @param next    - next node for version
  /// @param cmp     - comparator that compares versions
  ListNode(int version, const T& value, std::shared_ptr<ListNode> last, std::shared_ptr<ListNode> next,
           CmpByListVersion cmp)
      : next_(std::map<int, std::shared_ptr<ListNode>, CmpByListVersion>(cmp)),
        last_(std::map<int, std::shared_ptr<ListNode>, CmpByListVersion>(cmp)),
        value_(std::map<int, T, CmpByListVersion>(cmp))
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
  ListNode(int version, std::shared_ptr<ListNode> last,
           std::shared_ptr<ListNode> next, CmpByListVersion cmp)
      : next_(std::map<int, std::shared_ptr<ListNode>, CmpByListVersion>(cmp)),
        last_(std::map<int, std::shared_ptr<ListNode>, CmpByListVersion>(cmp)),
        value_(std::map<int, T, CmpByListVersion>(cmp)) {
    next_[version] = next;
    last_[version] = last;
  }

  bool operator==(const ListNode &other) const {
    return next_ == other.next_ && last_ == other.last_ && value_ == other.value_;
  }

  bool operator!=(const ListNode &other) const {
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
  bool setNext(int version, std::shared_ptr<ListNode> next) { 
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
  bool setLast(int version, std::shared_ptr<ListNode> last) { 
      if (!canSetLast() && last_.find(version) == last_.end()) {
        return false;
      }
      last_[version] = last;
      return true;
  }

  /// copy from the src node all the following nodes after version
  /// @param src     - the node from which we copy
  /// @param version - the version from which it copies
  void copyNextAfter(std::shared_ptr<ListNode> src, int version) { 
      for (auto i = src->next_.lower_bound(version); i != src->next_.end(); ++i) {
        this->next_[i->first] = src->next_[i->first];
      }
  }

  /// copy from src node, all previous nodes after version
  /// @param src     - the node from which we copy
  /// @param version - the version from which it copies
   void copyLastAfter(std::shared_ptr<ListNode> src, int version) {
     for (auto i = src->last_.lower_bound(version); i != src->last_.end(); ++i) {
      this->last_[i->first] = src->last_[i->first];
     }
  }

  /// @param version - version
  /// @return a value for version
  T find(int version) const {
    assert(!value_.empty(), "value_ in ListNode is empty. It is strange");
    auto it = value_.upper_bound(version);
    --it;
    return it -> second;
  }

  /// @param version - version
  /// @return the following node for the version
  std::shared_ptr<ListNode> getNext(int version) const {
    assert(!next_.empty(), "next_ in ListNode is empty. It is strange");
    auto it = next_.upper_bound(version);
    --it;
    return it->second;
  }

  /// @param version - version
  /// @return the previous node for the version version
  std::shared_ptr<ListNode> getLast(int version) const {
    assert(!last_.empty(), "last_ in ListNode is empty. It is strange");
    auto it = last_.upper_bound(version);
    --it;
    return it->second;
  }
};

/// iterator for the list
template <class T> class ListIterator final { 
  int version_;
  std::shared_ptr<ListNode<T>> node_;

  public:
  /// @param version - version
  /// @param node    - the node pointed to by the iterator
  ListIterator(int version, const std::shared_ptr<ListNode<T>> &node)
      : version_(version), node_(node) {}

  T operator*() const { return node_->find(version_); }
  ListNode<T> operator->() const { return node_; }

  ListIterator &operator++() {
    node_ = node_->getNext(version_);
    return *this;
  }

  ListIterator& operator--() {
    node_ = node_->getLast(version_);
    return *this;
  }

  ListIterator operator++(int) {
    ListIterator tmp = *this;
    node_ = node_->getNext(version_);
    return tmp;
  }

  ListIterator &operator--(int) {
    ListIterator tmp = *this;
    node_ = node_->getLast(version_);
    return tmp;
  }

  bool operator==(const ListIterator &other) const {
    return version_ == other.version_ && node_ == other.node_;
  }

  bool operator!=(const ListIterator &other) const {
    return version_ != other.version_ || node_ != other.node_;
  }
};

/// reverse iterator for list
template <class T> class ListReverseIterator final {
  int version_;
  std::shared_ptr<ListNode<T>> node_;

public:
  /// @param version - the version for which the list is
  /// @param node    - the node pointed to by the iterator
  ListReverseIterator(int version, const std::shared_ptr<ListNode<T>> &node)
      : version_(version), node_(node) {}

  T operator*() const { return node_->find(version_); }
  ListNode<T> operator->() const { return node_; }

  ListReverseIterator &operator--() {
    node_ = node_->getNext(version_);
    return *this;
  }

  ListReverseIterator &operator++() {
    node_ = node_->getLast(version_);
    return *this;
  }

  ListReverseIterator operator--(int) {
    node_ = node_->getNext(version_);
    return *this;
  }

  ListReverseIterator &operator++(int) {
    node_ = node_->getLast(version_);
    return *this;
  }

  bool operator==(const ListReverseIterator &other) const {
    return version_ == other.version_ && node_ == other.node_;
  }

  bool operator!=(const ListReverseIterator &other) const {
    return version_ != other.version_ || node_ != other.node_;
  }
};

/// version of persistent list
template <class T>
class PersistentList final : public UndoablePersistentCollection<PersistentList<T>> {
private:
  /// version
  int version_;
  std::shared_ptr<ListOrder> listOrder_;
  /// special node without value
  std::shared_ptr<ListNode <T>> head_;
  /// special node without value
  std::shared_ptr<ListNode <T>> tail_;
  /// size
  size_t size_;
  /// to undo/redo
  Undo::UndoRedoManager<PersistentList> undoRedoManager_;

  /// @param version   - version
  /// @param listOrder - structure for version comparison
  /// @param head      - node that holds the first nodes in all versions
  /// @param tail      - node that keeps the latest nodes in all versions
  /// @param size      - length of this version of the list
  PersistentList(int version, std::shared_ptr<ListOrder> listOrder, std::shared_ptr<ListNode <T>> head, std::shared_ptr<ListNode<T>> tail, size_t size,
                 Undo::UndoRedoManager<PersistentList> undoRedoManager)
      : version_(version), listOrder_(listOrder), head_(head), tail_(tail), size_(size),
        undoRedoManager_(undoRedoManager) {
  }
  
  /// @param new_version  - version for new list
  /// @param size         - size for new list
  /// @return list with version_ new_version, size_ size and current head_ and tail_
  PersistentList<T> getChildren(int new_version, size_t size) const {
    const auto undo = [version = version_, listOrder = listOrder_, head = head_, tail = tail_, size_undo = size_](auto manager) {
      return PersistentList{version, listOrder, head, tail, size_undo, manager};
    };

    const auto redo = [version = new_version, listOrder = listOrder_, head = head_, tail = tail_, size_undo = size](auto manager) {
      return PersistentList{version, listOrder, head, tail, size_undo, manager};
    };
    Undo::UndoRedoManager<PersistentList> newUndoRedoManager = undoRedoManager_.pushUndo(Undo::createAction<PersistentList>(undo, redo));
    return PersistentList<T>(new_version, listOrder_, head_, tail_, size, newUndoRedoManager);
  }

  /// @param version - version
  /// @param index   - index
  /// @return node with index number in version list
  std::shared_ptr<ListNode <T>> findNodeByIndex(int version, int index) const {
    if (index >= size_) {
      throw std::exception("index more size");
    }
    std::shared_ptr<ListNode <T>> ptr = head_;
    ++index; //потому что head_ без знечения
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
  std::shared_ptr<ListNode <T>> findNodeByIndex(int index) const {
    return findNodeByIndex(version_, index);
  }

  /// makes a new node when the old one gets too fat
  /// @param version - version with which the new node is added
  /// @param value   - value that this node will have on version version
  /// @param last    - node to which we insert
  /// @param next    - node after which we insert
  void makeNewNode(int version, const T &value, std::shared_ptr<ListNode <T>> last,
                   std::shared_ptr<ListNode <T>> next) {
    std::shared_ptr<ListNode <T>> new_node =
        std::make_shared<ListNode <T>>(version, value, nullptr, nullptr, CmpByListVersion(listOrder_));
    auto cur_last = last;
    auto cur_next = new_node;
    while (!cur_last->canSetNext()) {
      std::shared_ptr<ListNode <T>> cur_new_node = std::make_shared<ListNode <T>>(version, cur_last->find(version), 
          cur_last->getLast(version), cur_next, CmpByListVersion(listOrder_));
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
      std::shared_ptr<ListNode <T>> cur_new_node =
          std::make_shared<ListNode <T>>(version, cur_next->find(version), cur_last,
              cur_next->getNext(version), CmpByListVersion(listOrder_));
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
  void dropNode(int version, int old_version, const std::shared_ptr<ListNode <T>>& new_node) {
    auto cur_last = new_node->getLast(old_version);
    auto cur_next = new_node->getNext(old_version);
    while (!cur_last->canSetNext()) {
      std::shared_ptr<ListNode <T>> cur_new_node = std::make_shared<ListNode <T>>(
          version, cur_last->find(old_version), cur_last->getLast(old_version),
                                     cur_next, CmpByListVersion(listOrder_));
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
      std::shared_ptr<ListNode <T>> cur_new_node =
          std::make_shared<ListNode <T>>(version, cur_next->find(old_version), cur_last,
                                     cur_next->getNext(old_version), CmpByListVersion(listOrder_));
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
    listOrder_ = std::make_shared<ListOrder>();
    listOrder_ -> add(0);
    head_ = std::make_shared<ListNode <T>>(version_, nullptr, nullptr, CmpByListVersion(listOrder_));
    tail_ = std::make_shared<ListNode <T>>(version_, head_, nullptr, CmpByListVersion(listOrder_));
    head_->setNext(1, tail_);
    size_ = 0;
  }

  /// create a list based on a list
  PersistentList(const std::initializer_list<T>& v) : version_(1) {
    listOrder_ = std::make_shared<ListOrder>();
    listOrder_->add(0);
    head_ = std::make_shared<ListNode <T>>(version_, nullptr, nullptr, CmpByListVersion(listOrder_));
    std::shared_ptr<ListNode <T>> ptr = head_;
    for (auto i : v) {
      auto fatNode = std::make_shared<ListNode <T>>(version_, i, ptr, nullptr, CmpByListVersion(listOrder_));
      ptr->setNext(version_, fatNode);
      fatNode->setLast(version_, ptr);
      ptr = fatNode;
    }
    tail_ = std::make_shared<ListNode <T>>(version_, ptr, nullptr, CmpByListVersion(listOrder_));
    ptr->setNext(version_, tail_);
    size_ = v.size();
  }

  /// find value by index
  /// @param index - index
  T find(int index) const { 
      std::shared_ptr<ListNode <T>> ptr = findNodeByIndex(index);
      return ptr->find(version_);
  }

  /// set value by index
  /// @param index - index
  /// @param value - value
  PersistentList set(int index, const T& value) { 
    std::shared_ptr<ListNode <T>> ptr = findNodeByIndex(index);
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
    std::shared_ptr<ListNode <T>> ptr = findNodeByIndex(index);
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
     std::shared_ptr<ListNode <T>> ptr = findNodeByIndex(index);
     auto last = ptr->getLast(version_);
     makeNewNode(new_version, value, last, ptr);
     auto ptr1 = findNodeByIndex(new_version, index);
     dropNode(-new_version, new_version, ptr1);
     return getChildren(new_version, size_ + 1); 
  }

  /// add value to head
  /// @param value - value to add
  PersistentList push_front(const T &value) {
      return insert(0, value);
  }

  /// add value to tail
  /// @param value - value to add
  PersistentList push_back(const T &value) {
    int new_version = listOrder_->add(version_);
    auto last = tail_->getLast(version_);
    auto next = tail_;
    makeNewNode(new_version, value, last, next);
    auto ptr1 = tail_->getLast(new_version);
    dropNode(-new_version, new_version, ptr1);
    return getChildren(new_version, size_ + 1);
  }

  /// remove from head
  PersistentList pop_front() { return erase(0); }

  /// remove from tail
  PersistentList pop_back() { return erase(size_ - 1); }

  /// roll back a version
  PersistentList<T> undo() const { 
    CONTRACT_EXPECT(undoRedoManager_.hasUndo());
    return undoRedoManager_.undo();
  }

  /// move forward version
  PersistentList<T> redo() const {
    CONTRACT_EXPECT(undoRedoManager_.hasRedo());
    return undoRedoManager_.redo();
  }

  ListIterator<T> begin() const { return ListIterator<T>(version_, head_->getNext(version_)); }

  ListIterator<T> end() const { return ListIterator<T>(version_, tail_); }

  ListReverseIterator<T> rbegin() const {
    return ListReverseIterator<T>(version_, tail_->getLast(version_));
  }

  ListReverseIterator<T> rend() const { return ListReverseIterator<T>(version_, head_); }

  /// list size
  size_t size() const { return size_;}

};
}
#endif
