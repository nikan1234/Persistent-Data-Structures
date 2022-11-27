#ifndef PERSISTENT_LIST_H
#define PERSISTENT_LIST_H

#include <Undo/UndoRedoManager.h>

#include <vector>
#include <list>
#include <algorithm>
#include <memory>
#include <cassert>
#include <map>
#include <cmath>

namespace Persistence {
// простая версия структуры, определяющей порядок
// необходимо для полной, а не частичной персистентности
struct ListOrder {
  const double weight_border = 2000000000000;
  std::list<int> list;
  std::vector<std::list<int>::iterator> handles;
  std::vector<double> weight_true;
  std::vector<double> weight_reverse;
  ListOrder(){};

  // добавить версию и противоположную ей версию
  // parent - версия-родитель для новой версии. При первом вызове аргумент не учитывается
  // возвращает номер новой версии
  int add(int parent) {
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
    //если равны, сделать пересчёт весов по list
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

  // определяет, предшествует ли версия l версии r в списке версий
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

//компаратор, чтобы сравнивать версии для полной персистентности
struct CmpByListVersion {
  std::shared_ptr<ListOrder> listOrder_;
  CmpByListVersion(std::shared_ptr<ListOrder> listOrder) : listOrder_(listOrder) {}

  bool operator()(const int a, const int b) const { 
      return listOrder_ -> less(a, b); }
};

template <class T> struct ListNode {
  static const int MAX_SIZE_FAT_NODE = 10;

  std::map<int, std::shared_ptr<ListNode>, CmpByListVersion> next_;
  std::map<int, std::shared_ptr<ListNode>, CmpByListVersion> last_;
  std::map<int, T, CmpByListVersion> value_;

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

  // конструктор для head_ и tail_ - специальных узлов, связывающих версии
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

  bool add(int version, const T& value) {
    if (value_.size() >= MAX_SIZE_FAT_NODE) {
      return false;
    }
    value_[version] = value;
    return true;
  }

  // будем в head и tail добавлять в любом случае value_.size() == 0 
  bool canSetNext() { return value_.size() == 0 || next_.size() < MAX_SIZE_FAT_NODE; }

  bool canSetLast() { return value_.size() == 0 || last_.size() < MAX_SIZE_FAT_NODE; }

  bool setNext(int version, std::shared_ptr<ListNode> next) { 
      if (!canSetNext() && next_.find(version) == next_.end()) {
        return false;
      }
      next_[version] = next; 
      return true;
  }

  bool setLast(int version, std::shared_ptr<ListNode> last) { 
      if (!canSetLast() && last_.find(version) == last_.end()) {
        return false;
      }
      last_[version] = last;
      return true;
  }

  void copyNextAfter(std::shared_ptr<ListNode> src, int version) { 
      for (auto i = src->next_.lower_bound(version); i != src->next_.end(); ++i) {
        this->next_[i->first] = src->next_[i->first];
      }
  }

   void copyLastAfter(std::shared_ptr<ListNode> src, int version) {
     for (auto i = src->last_.lower_bound(version); i != src->last_.end(); ++i) {
      this->last_[i->first] = src->last_[i->first];
     }
  }

  T find(int version) const {
    assert(!value_.empty(), "value_ in ListNode is empty. It is strange");
    auto it = value_.upper_bound(version);
    --it;
    return it -> second;
  }

  std::shared_ptr<ListNode> getNext(int version) const {
    assert(!next_.empty(), "next_ in ListNode is empty. It is strange");
    auto it = next_.upper_bound(version);
    --it;
    return it->second;
  }

  std::shared_ptr<ListNode> getLast(int version) const {
    assert(!last_.empty(), "last_ in ListNode is empty. It is strange");
    auto it = last_.upper_bound(version);
    --it;
    return it->second;
  }
};

template <class T> class ListIterator final { 
  int version_;
  std::shared_ptr<ListNode<T>> node_;

  public:
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
    node_ = node_->getNext(version_);
    return *this;
  }

  ListIterator &operator--(int) {
    node_ = node_->getLast(version_);
    return *this;
  }

  bool operator==(const ListIterator &other) const {
    return version_ == other.version_ && node_ == other.node_;
  }

  bool operator!=(const ListIterator &other) const {
    return version_ != other.version_ || node_ != other.node_;
  }
};

template <class T> class ListReverseIterator final {
  int version_;
  std::shared_ptr<ListNode<T>> node_;

public:
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

template <class T> class PersistentList final : public Undo::IUndoable<PersistentList<T>> {
private:
  int version_;
  std::shared_ptr<ListOrder> listOrder_;
  // head_, tail_ - специальный ноды без значений
  std::shared_ptr<ListNode <T>> head_;
  std::shared_ptr<ListNode <T>> tail_;
  size_t size_;
  Undo::UndoRedoManager<PersistentList> undoRedoManager_;

  PersistentList(int version, std::shared_ptr<ListOrder> listOrder, std::shared_ptr<ListNode <T>> head, std::shared_ptr<ListNode<T>> tail, size_t size_,
                 Undo::UndoRedoManager<PersistentList> undoRedoManager)
      : version_(version), listOrder_(listOrder), head_(head), tail_(tail), size_(size_),
        undoRedoManager_(undoRedoManager) {
  }

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

  std::shared_ptr<ListNode <T>> findNodeByIndex(int index) const {
    return findNodeByIndex(version_, index);
  }

  // делает новую Node, когда старая становится слишком толстой
  void makeNewNode(int version, const T &value, std::shared_ptr<ListNode <T>> last,
                   std::shared_ptr<ListNode <T>> next) {
    std::shared_ptr<ListNode <T>> new_node =
        std::make_shared<ListNode <T>>(version, value, nullptr, nullptr, CmpByListVersion(listOrder_));
    auto cur_last = last;
    auto cur_next = new_node;
    while (!cur_last->canSetNext()) {
      std::shared_ptr<ListNode <T>> cur_new_node = std::make_shared<ListNode <T>>(version, cur_last->find(version), 
          cur_last->getLast(version), cur_next, CmpByListVersion(listOrder_));
      // todo copy
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
      // todo copy
      cur_new_node->copyLastAfter(cur_next, version);
      cur_next->getNext(version)->setLast(version, cur_new_node);
      cur_last->setNext(version, cur_new_node);
      cur_last = cur_new_node;
      cur_next = cur_next->getNext(version);
    }
    cur_last->setNext(version, cur_next);
    cur_next->setLast(version, cur_last);
  }

  // todo check
  void dropNode(int version, int old_version, const std::shared_ptr<ListNode <T>>& new_node) {
    auto cur_last = new_node->getLast(old_version);
    auto cur_next = new_node->getNext(old_version);
    while (!cur_last->canSetNext()) {
      std::shared_ptr<ListNode <T>> cur_new_node = std::make_shared<ListNode <T>>(
          version, cur_last->find(old_version), cur_last->getLast(old_version),
                                     cur_next, CmpByListVersion(listOrder_));
      //todo copy
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
      // todo copy
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

  T find(int index) const { 
      std::shared_ptr<ListNode <T>> ptr = findNodeByIndex(index);
      return ptr->find(version_);
  }

  PersistentList set(int index, const T& value) { 
    std::shared_ptr<ListNode <T>> ptr = findNodeByIndex(index);
    int new_version = listOrder_ -> add(version_);
    // todo избавиться от fatnode
    // если в ноде ещё есть место добавляем
    if (!ptr->add(new_version, value)) {
      makeNewNode(new_version, value, ptr->getLast(version_), ptr->getNext(version_));
    }
    if (!ptr->add(-new_version, ptr->find(version_))) {
      makeNewNode(-new_version, ptr->find(version_), ptr->getLast(version_), ptr->getNext(version_));
    }
    return getChildren(new_version, size_); 
    //todo подвязать себя
    //ListNode <T> ListNode <T>(new_version, value, ptr->getLast(version_), ptr->getNext(version_),
    //                  CmpByListVersion(listOrder_)); 
    //return GetChildren(new_version);
  }

  // нужно реализовывать откат
  PersistentList erase(int index) { 
    std::shared_ptr<ListNode <T>> ptr = findNodeByIndex(index);
    auto last = ptr->getLast(version_);
    auto next = ptr->getNext(version_);
    int new_version = listOrder_->add(version_);
    //last->setNext(new_version, next);
    //next->SetLast(new_version, last);
    dropNode(new_version, version_, ptr);
    //last->setNext(-new_version, ptr);
    //next->SetLast(-new_version, ptr);
    auto ptr1 = findNodeByIndex(version_, index);
    makeNewNode(-new_version, ptr1->find(version_), last, next);
    return getChildren(new_version, size_ - 1); 
  }

  // будем добавлять до индекса
  PersistentList insert(int index, const T& value) { 
     int new_version = listOrder_->add(version_);
     std::shared_ptr<ListNode <T>> ptr = findNodeByIndex(index);
     auto last = ptr->getLast(version_);
     auto next = ptr->getNext(version_);
     makeNewNode(new_version, value, last, ptr);
     auto ptr1 = findNodeByIndex(new_version, index);
     dropNode(-new_version, new_version, ptr1);
     return getChildren(new_version, size_ + 1); 
  }

  PersistentList<T> undo() const { 
    CONTRACT_EXPECT(undoRedoManager_.hasUndo());
    return undoRedoManager_.undo();
  }

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
};
}// namespace Persistence
#endif
