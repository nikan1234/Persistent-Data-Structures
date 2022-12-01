#include <Collections/PersistentList.h>

#include <gtest/gtest.h>

using namespace Persistence;

// check that the version order is determined correctly
TEST(PersistentListTests, Order) { 
	ListOrder order; 
	ASSERT_EQ(order.add(1), 1);
    ASSERT_EQ(order.add(1), 2);
    ASSERT_EQ(order.add(1), 3);
    ASSERT_EQ(order.add(2), 4);
    ASSERT_EQ(order.add(2), 5);
    ASSERT_EQ(order.add(3), 6);
    ASSERT_EQ(order.add(5), 7);
    ASSERT_EQ(order.add(7), 8);
    ASSERT_EQ(order.less(1, 2), true);
    ASSERT_EQ(order.less(1, -1), true);
    ASSERT_EQ(order.less(2, -2), true);
    ASSERT_EQ(order.less(1, 1), false);
    ASSERT_EQ(order.less(4, 3) && order.less(3, 6), order.less(4, 6));
}

// check that no more than ListNode::MAX_SIZE_FAT_NODE will be added to one node
TEST(PersistentListTests, AddNode) { 
    std::shared_ptr<ListOrder> listOrder = std::make_shared<ListOrder>(); 
    listOrder->add(1);
    ListNode<int> listNode(1, 10, nullptr, nullptr, CmpByListVersion(listOrder)); 
    for (int i = 1; i < ListNode<int>::MAX_SIZE_FAT_NODE; ++i) {
      ASSERT_EQ(listNode.add(listOrder->add(i), i), true);
    }
    ASSERT_EQ(listNode.add(listOrder->add(ListNode<int>::MAX_SIZE_FAT_NODE), 10), false);
}

// we check that in one node the value is correctly searched for by version
TEST(PersistentListTests, FindNode) {
  std::shared_ptr<ListOrder> listOrder = std::make_shared<ListOrder>();
  listOrder->add(1);
  ListNode<int> listNode(1, 10, nullptr, nullptr, CmpByListVersion(listOrder));
  listNode.add(listOrder->add(1), 11); // 2
  listNode.add(listOrder->add(1), 12); // 3
  listNode.add(listOrder->add(2), 13); // 4
  listNode.add(listOrder->add(1), 14); // 5
  listNode.add(listOrder->add(2), 15); // 6
  listOrder->add(4); // change for version 7 is not in this node
  listOrder->add(6); // change for version 8 is not in this node
  ASSERT_EQ(listNode.find(3), 12);
  ASSERT_EQ(listNode.find(4), 13);
  ASSERT_EQ(listNode.find(7), 13);
  ASSERT_EQ(listNode.find(8), 15);

  ListNode<int> listNode1(3, 10, nullptr, nullptr, CmpByListVersion(listOrder));
  listNode1.add(7, 11);
  ASSERT_EQ(listNode1.find(4), 10);
}

// test to find an element in a list
TEST(PersistentListTests, FindList) {
  const PersistentList<int> test({1, 2, 3, 4});
  ASSERT_EQ(test.find(0), 1);
  ASSERT_EQ(test.find(1), 2);
  ASSERT_EQ(test.find(2), 3);
  ASSERT_EQ(test.find(3), 4);
}

// index test
TEST(PersistentListTests, SetList) {
  PersistentList<int> v1({1, 2, 3, 4});
  auto v2 = v1.set(0, -1);
  auto v3 = v1.set(1, -2);
  auto v4 = v2.set(2, -3);
  //                v1 {1, 2, 3, 4}
  //                   /           \
  //         v2 {-1, 2, 3, 4}     v3 {1, -2, 3, 4}
  //                 /
  //    v4 {-1, 2, -3, 4}
  ASSERT_EQ(v2.find(0), -1);
  ASSERT_EQ(v3.find(0), 1);
  ASSERT_EQ(v3.find(1), -2);
  ASSERT_EQ(v4.find(2), -3);
  ASSERT_EQ(v4.find(0), -1);
}

// deletion test
TEST(PersistentListTests, EraseList) {
  PersistentList<int> v1({1, 2, 3, 4});
  auto v2 = v1.erase(1);
  auto v3 = v1.erase(2);
  auto v4 = v2.erase(2);
  auto v5 = v4.erase(0);
  //            v1 {1, 2, 3, 4}
  //             /           \
  //         v2 {1, 3, 4}     v3 {1, 2, 4}
  //          /
  //    v4 {1, 3}
  //        /
  //    v5 {3}
  ASSERT_EQ(v2.find(0), 1);
  ASSERT_EQ(v2.find(1), 3);
  ASSERT_EQ(v3.find(0), 1);
  ASSERT_EQ(v3.find(1), 2);
  ASSERT_EQ(v3.find(2), 4);
  ASSERT_EQ(v4.find(0), 1);
  ASSERT_EQ(v4.find(1), 3);
  ASSERT_ANY_THROW(v4.find(2), 4);
  ASSERT_EQ(v5.find(0), 3);
}

// list insertion test
TEST(PersistentListTests, InsertList) {
  PersistentList<int> v1({1, 2, 3, 4});
  auto v2 = v1.insert(1, 5);
  auto v3 = v1.insert(1, 6);
  auto v4 = v2.insert(1, 7);
  auto v5 = v4.insert(0, 8);
  //                       v1 {1, 2, 3, 4}
  //                       /           \
  //         v2 {1, 5, 2, 3, 4}     v3 {1, 6, 2, 3, 4}
  //                     /               
  //    v4 {1, 7, 5, 2, 3, 4}       
  //                   /
  //    v5 {8, 1, 7, 5, 2, 3, 4}
  ASSERT_EQ(v2.find(0), 1);
  ASSERT_EQ(v2.find(1), 5);
  ASSERT_EQ(v2.find(2), 2);
  ASSERT_EQ(v2.find(3), 3);
  ASSERT_EQ(v2.find(4), 4);
  ASSERT_EQ(v3.find(0), 1);
  ASSERT_EQ(v3.find(1), 6);
  ASSERT_EQ(v3.find(2), 2);
  ASSERT_EQ(v3.find(3), 3);
  ASSERT_EQ(v3.find(4), 4);
  ASSERT_EQ(v4.find(0), 1);
  ASSERT_EQ(v4.find(1), 7);
  ASSERT_EQ(v4.find(2), 5);
  ASSERT_EQ(v4.find(3), 2);
  ASSERT_EQ(v4.find(4), 3);
  ASSERT_EQ(v4.find(5), 4);
  ASSERT_EQ(v5.find(0), 8);
  ASSERT_EQ(v5.find(1), 1);
  ASSERT_EQ(v5.find(2), 7);
  ASSERT_EQ(v5.find(3), 5);
  ASSERT_EQ(v5.find(4), 2);
  ASSERT_EQ(v5.find(5), 3);
  ASSERT_EQ(v5.find(6), 4);
}

// undo redo test
TEST(PersistentListTests, UndoList) {
  PersistentList<int> v1({1, 2, 3, 4});
  auto v2 = v1.insert(1, 5);
  auto v2_undo = v2.undo();
  auto v3 = v1.insert(1, 6);
  auto v4 = v2.insert(1, 7);
  auto v4_undo = v4.undo();
  auto v4_undo_undo = v4_undo.undo();
  auto v4_undo_undo_redo = v4_undo_undo.redo();
  //                       v1 {1, 2, 3, 4}
  //                       /           \
  //         v2 {1, 5, 2, 3, 4}     v3 {1, 6, 2, 3, 4}
  //                     /               
  //    v4 {1, 7, 5, 2, 3, 4}      
  ASSERT_EQ(v2_undo.find(1), 2);
  ASSERT_EQ(v4_undo_undo.find(1), 2);
  ASSERT_EQ(v4_undo_undo_redo.find(1), 5);
}

// iterator dereference test
TEST(PersistentListTests, IteratorValueTest) {
  std::shared_ptr<ListOrder> listOrder = std::make_shared<ListOrder>();
  listOrder->add(1);
  ListIterator<int> iterator(1, std::make_shared<ListNode<int>>(1, 10, nullptr, nullptr, CmpByListVersion(listOrder)));
  ASSERT_EQ(*iterator, 10);
}

// iterator traversal test
TEST(PersistentListTests, IteratorSumList) {
  PersistentList<int> v1({1, 2, 3, 4});
  auto v2 = v1.set(0, -1);
  //                v1 {1, 2, 3, 4}
  //                   /           
  //         v2 {-1, 2, 3, 4}    
  int sum = 0;
  for (auto i = v2.begin(); i != v2.end(); ++i) {
    sum += *i;
  }
  ASSERT_EQ(sum, 8);
  sum = 0;
  for (auto i = v1.begin(); i != v1.end(); ++i) {
    sum += *i;
  }
  ASSERT_EQ(sum, 10);
}

// backtracking test with iterator
TEST(PersistentListTests, ReverseIteratorSumList) {
  PersistentList<int> v1({1, 2, 3, 4});
  auto v2 = v1.set(0, -1);
  //                v1 {1, 2, 3, 4}
  //                   /
  //         v2 {-1, 2, 3, 4}
  int sum = 0;
  for (auto i = v2.rbegin(); i != v2.rend(); ++i) {
    sum += *i;
  }
  ASSERT_EQ(sum, 8);
  sum = 0;
  for (auto i = v1.rbegin(); i != v1.rend(); ++i) {
    sum += *i;
  }
  ASSERT_EQ(sum, 10);
}

// test for postfix, prefix for list iterator
TEST(PersistentListTests, PostfixPrefixIteratorList) {
  PersistentList<int> v1({1, 2, 3, 4});
  auto v2 = v1.set(0, -1);
  //                v1 {1, 2, 3, 4}
  //                   /
  //         v2 {-1, 2, 3, 4}
  auto pre = v2.begin()++;
  auto post = ++v2.begin();
  ASSERT_EQ(*pre, -1);
  ASSERT_EQ(*post, 2);
}

// list length test
TEST(PersistentListTests, SizeList) {
  PersistentList<int> v1({1, 2, 3, 4});
  auto v2 = v1.insert(1, 5);
  auto v2_undo = v2.undo();
  auto v3 = v1.insert(1, 6);
  auto v4 = v2.insert(1, 7);
  auto v4_undo = v4.undo();
  auto v4_undo_undo = v4_undo.undo();
  auto v4_undo_undo_redo = v4_undo_undo.redo();
  //                       v1 {1, 2, 3, 4}
  //                       /           \
  //         v2 {1, 5, 2, 3, 4}     v3 {1, 6, 2, 3, 4}
  //                     /
  //    v4 {1, 7, 5, 2, 3, 4}
  ASSERT_EQ(v2_undo.size(), 4);
  ASSERT_EQ(v4_undo_undo.size(), 4);
  ASSERT_EQ(v4_undo_undo_redo.size(), 5);
}

// test for adding to the end and beginning of the list
TEST(PersistentListTests, PustFrontPushBackList) {
  PersistentList<int> v1({1, 2, 3, 4});
  auto v2 = v1.push_front(5);
  auto v3 = v1.push_back(6);
  //                       v1 {1, 2, 3, 4}
  //                       /            \
  //         v2 {5, 1, 2, 3, 4}      v3 {1, 2, 3, 4, 6}
  ASSERT_EQ(v2.find(0), 5);
  ASSERT_EQ(v3.find(4), 6);
}

// test to remove from the end and the beginning of the list
TEST(PersistentListTests, PopFrontPopBackList) {
  PersistentList<int> v1({1, 2, 3, 4});
  auto v2 = v1.pop_front();
  auto v3 = v1.pop_back();
  //                       v1 {1, 2, 3, 4}
  //                       /            \
  //         v2 {2, 3, 4}      v3 {1, 2, 3}
  ASSERT_EQ(v2.find(0), 2);
  ASSERT_ANY_THROW(v3.find(3), 4);
}