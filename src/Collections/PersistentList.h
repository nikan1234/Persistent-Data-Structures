#ifndef PERSISTENT_LIST_H
#define PERSISTENT_LIST_H
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

typedef int T;

struct ListNode {
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

  bool Add(int version, const T& value) {
    if (value_.size() >= MAX_SIZE_FAT_NODE) {
      return false;
    }
    value_[version] = value;
    return true;
  }

  // будем в head и tail добавлять в любом случае value_.size() == 0 
  bool CanSetNext() { return value_.size() == 0 || next_.size() < MAX_SIZE_FAT_NODE; }

  bool CanSetLast() { return value_.size() == 0 || last_.size() < MAX_SIZE_FAT_NODE; }

  bool SetNext(int version, std::shared_ptr<ListNode> next) { 
      if (!CanSetNext() && next_.find(version) == next_.end()) {
        return false;
      }
      next_[version] = next; 
      return true;
  }

  bool SetLast(int version, std::shared_ptr<ListNode> last) { 
      if (!CanSetLast() && last_.find(version) == last_.end()) {
        return false;
      }
      last_[version] = last;
      return true;
  }

  void CopyNextAfter(std::shared_ptr<ListNode> src, int version) { 
      for (auto i = src->next_.lower_bound(version); i != src->next_.end(); ++i) {
        this->next_[i->first] = src->next_[i->first];
      }
  }

   void CopyLastAfter(std::shared_ptr<ListNode> src, int version) {
     for (auto i = src->last_.lower_bound(version); i != src->last_.end(); ++i) {
      this->last_[i->first] = src->last_[i->first];
     }
  }

  T Find(int version) const {
    assert(!value_.empty(), "value_ in ListNode is empty. It is strange");
    auto it = value_.upper_bound(version);
    --it;
    return it -> second;
  }

  std::shared_ptr<ListNode> GetNext(int version) const {
    assert(!next_.empty(), "next_ in ListNode is empty. It is strange");
    auto it = next_.upper_bound(version);
    --it;
    return it->second;
  }

  std::shared_ptr<ListNode> GetLast(int version) const {
    assert(!last_.empty(), "last_ in ListNode is empty. It is strange");
    auto it = last_.upper_bound(version);
    --it;
    return it->second;
  }
};

class PersistentList final {
private:
  int version_;
  std::shared_ptr<ListOrder> listOrder_;
  // head_, tail_ - специальный ноды без значений
  std::shared_ptr<ListNode> head_;
  std::shared_ptr<ListNode> tail_;
  size_t size_;

  PersistentList(int version, std::shared_ptr<ListOrder> listOrder, std::shared_ptr<ListNode> head,
                 std::shared_ptr<ListNode> tail, size_t size_)
      : version_(version), listOrder_(listOrder), head_(head), tail_(tail), size_(size_) {
  }

  PersistentList GetChildren(int new_version, size_t size) {
    return PersistentList(new_version, listOrder_, head_, tail_, size);
  }

  std::shared_ptr<ListNode> FindNodeByIndex(int version, int index) const {
    if (index >= size_) {
      throw std::exception("index more size");
    }
    std::shared_ptr<ListNode> ptr = head_;
    ++index; //потому что head_ без знечения
    for (int i = 0; i < index; ++i) {
      if (ptr == nullptr) {
        throw std::exception("not value by index for current version");
      }
      ptr = ptr->GetNext(version);
    }
    if (ptr == nullptr) {
      throw std::exception("not value by index for current version");
    }
    return ptr;
  }

  std::shared_ptr<ListNode> FindNodeByIndex(int index) const {
    return FindNodeByIndex(version_, index);
  }

  // делает новую Node, когда старая становится слишком толстой
  void MakeNewNode(int version, const T &value, std::shared_ptr<ListNode> last,
                   std::shared_ptr<ListNode> next) {
    std::shared_ptr<ListNode> new_node =
        std::make_shared<ListNode>(version, value, nullptr, nullptr, CmpByListVersion(listOrder_));
    auto cur_last = last;
    auto cur_next = new_node;
    while (!cur_last->CanSetNext()) {
      std::shared_ptr<ListNode> cur_new_node = std::make_shared<ListNode>(version, cur_last->Find(version), 
          cur_last->GetLast(version), cur_next, CmpByListVersion(listOrder_));
      // todo copy
      cur_new_node->CopyNextAfter(cur_last, version);
      cur_last->GetLast(version)->SetNext(version, cur_new_node);
      cur_next->SetLast(version, cur_new_node);
      cur_next = cur_new_node;
      cur_last = cur_last->GetLast(version);
    }
    cur_last->SetNext(version, cur_next);
    cur_next->SetLast(version, cur_last);
    cur_next = next;
    cur_last = new_node;
    while (!cur_next->CanSetLast()) {
      std::shared_ptr<ListNode> cur_new_node =
          std::make_shared<ListNode>(version, cur_next->Find(version), cur_last,
              cur_next->GetNext(version), CmpByListVersion(listOrder_));
      // todo copy
      cur_new_node->CopyLastAfter(cur_next, version);
      cur_next->GetNext(version)->SetLast(version, cur_new_node);
      cur_last->SetNext(version, cur_new_node);
      cur_last = cur_new_node;
      cur_next = cur_next->GetNext(version);
    }
    cur_last->SetNext(version, cur_next);
    cur_next->SetLast(version, cur_last);
  }

  // todo check
  void DropNode(int version, int old_version, const std::shared_ptr<ListNode>& new_node) {
    auto cur_last = new_node->GetLast(old_version);
    auto cur_next = new_node->GetNext(old_version);
    while (!cur_last->CanSetNext()) {
      std::shared_ptr<ListNode> cur_new_node = std::make_shared<ListNode>(
          version, cur_last->Find(old_version), cur_last->GetLast(old_version),
                                     cur_next, CmpByListVersion(listOrder_));
      //todo copy
      cur_new_node->CopyNextAfter(cur_last, version);
      cur_last->GetLast(old_version)->SetNext(version, cur_new_node);
      cur_next->SetLast(version, cur_new_node);
      cur_next = cur_new_node;
      cur_last = cur_last->GetLast(old_version);
    }
    cur_last->SetNext(version, cur_next);
    cur_next->SetLast(version, cur_last);

    cur_last = new_node->GetLast(old_version);
    cur_next = new_node->GetNext(old_version);
    while (!cur_next->CanSetLast()) {
      std::shared_ptr<ListNode> cur_new_node =
          std::make_shared<ListNode>(version, cur_next->Find(old_version), cur_last,
                                     cur_next->GetNext(old_version), CmpByListVersion(listOrder_));
      // todo copy
      cur_new_node->CopyLastAfter(cur_next, version);
      cur_next->GetNext(old_version)->SetLast(version, cur_new_node);
      cur_last->SetNext(version, cur_new_node);
      cur_last = cur_new_node;
      cur_next = cur_next->GetNext(old_version);
    }
    cur_last->SetNext(version, cur_next);
    cur_next->SetLast(version, cur_last);
  }

public:
  PersistentList() : version_(1) { 
    listOrder_ = std::make_shared<ListOrder>();
    listOrder_ -> add(0);
    head_ = std::make_shared<ListNode>(version_, nullptr, nullptr, CmpByListVersion(listOrder_));
    tail_ = std::make_shared<ListNode>(version_, head_, nullptr, CmpByListVersion(listOrder_));
    head_->SetNext(1, tail_);
    size_ = 0;
  }

  PersistentList(const std::initializer_list<T>& v) : version_(1) {
    listOrder_ = std::make_shared<ListOrder>();
    listOrder_->add(0);
    head_ = std::make_shared<ListNode>(version_, nullptr, nullptr, CmpByListVersion(listOrder_));
    std::shared_ptr<ListNode> ptr = head_;
    for (auto i : v) {
      auto fatNode = std::make_shared<ListNode>(version_, i, ptr, nullptr, CmpByListVersion(listOrder_));
      ptr->SetNext(version_, fatNode);
      fatNode->SetLast(version_, ptr);
      ptr = fatNode;
    }
    tail_ = std::make_shared<ListNode>(version_, ptr, nullptr, CmpByListVersion(listOrder_));
    ptr->SetNext(version_, tail_);
    size_ = v.size();
  }

  T Find(int index) const { 
      std::shared_ptr<ListNode> ptr = FindNodeByIndex(index);
      return ptr->Find(version_);
  }

  PersistentList Set(int index, const T& value) { 
    std::shared_ptr<ListNode> ptr = FindNodeByIndex(index);
    int new_version = listOrder_ -> add(version_);
    // todo избавиться от fatnode
    // если в ноде ещё есть место добавляем
    if (!ptr->Add(new_version, value)) {
      MakeNewNode(new_version, value, ptr->GetLast(version_), ptr->GetNext(version_));
    }
    if (!ptr->Add(-new_version, ptr->Find(version_))) {
      MakeNewNode(-new_version, ptr->Find(version_), ptr->GetLast(version_), ptr->GetNext(version_));
    }
    return GetChildren(new_version, size_); 
    //todo подвязать себя
    //ListNode listNode(new_version, value, ptr->GetLast(version_), ptr->GetNext(version_),
    //                  CmpByListVersion(listOrder_)); 
    //return GetChildren(new_version);
  }

  // нужно реализовывать откат
  PersistentList Erase(int index) { 
    std::shared_ptr<ListNode> ptr = FindNodeByIndex(index);
    auto last = ptr->GetLast(version_);
    auto next = ptr->GetNext(version_);
    int new_version = listOrder_->add(version_);
    //last->SetNext(new_version, next);
    //next->SetLast(new_version, last);
    DropNode(new_version, version_, ptr);
    //last->SetNext(-new_version, ptr);
    //next->SetLast(-new_version, ptr);
    auto ptr1 = FindNodeByIndex(version_, index);
    MakeNewNode(-new_version, ptr1->Find(version_), last, next);
    return GetChildren(new_version, size_ - 1); 
  }

  // будем добавлять до индекса
  PersistentList Insert(int index, const T& value) { 
     int new_version = listOrder_->add(version_);
     std::shared_ptr<ListNode> ptr = FindNodeByIndex(index);
     auto last = ptr->GetLast(version_);
     auto next = ptr->GetNext(version_);
     MakeNewNode(new_version, value, last, ptr);
     auto ptr1 = FindNodeByIndex(new_version, index);
     DropNode(-new_version, new_version, ptr1);
     //std::shared_ptr<ListNode> new_node = std::make_shared<ListNode>(new_version, value, last, ptr, CmpByListVersion(listOrder_));
     //last->SetNext(new_version, new_node);
     //next->SetLast(new_version, new_node);
     // 
     //todo как переводвязать без создания новой ноды?
    // last->SetNext(-new_version, ptr);
    // next->SetLast(-new_version, ptr);
     return GetChildren(new_version, size_ + 1); 
  }
  // template <class T> class PersistentListIterator final {};
};
}// namespace Persistence
#endif
