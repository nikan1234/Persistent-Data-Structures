#include <Collections/PersistentList.h>

#include <gtest/gtest.h>

using namespace Persistence;

// проверяем, что порядок версий определяется корректно
TEST(PersistentListTests, Order) { 
	ListOrder order; 
	ASSERT_EQ(order.add(1), 1); //неважно, какой аргумент в первый раз
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

// проверяем, что в одну ноду добавится не больше ListNode::MAX_SIZE_FAT_NODE 
TEST(PersistentListTests, AddNode) { 
    std::shared_ptr<ListOrder> listOrder = std::make_shared<ListOrder>(); 
    listOrder->add(1);
    ListNode listNode(1, 10, nullptr, nullptr, CmpByListVersion(listOrder)); 
    for (int i = 1; i < ListNode::MAX_SIZE_FAT_NODE; ++i) {
      ASSERT_EQ(listNode.Add(listOrder->add(i), i), true);
    }
    ASSERT_EQ(listNode.Add(listOrder->add(ListNode::MAX_SIZE_FAT_NODE), 10), false);
}

// проверяем, что в одной ноде корректно ищется значение по версии
TEST(PersistentListTests, FindNode) {
  std::shared_ptr<ListOrder> listOrder = std::make_shared<ListOrder>();
  listOrder->add(1);
  ListNode listNode(1, 10, nullptr, nullptr, CmpByListVersion(listOrder));
  listNode.Add(listOrder->add(1), 11); // 2
  listNode.Add(listOrder->add(1), 12); // 3
  listNode.Add(listOrder->add(2), 13); // 4
  listNode.Add(listOrder->add(1), 14); // 5
  listNode.Add(listOrder->add(2), 15); // 6
  listOrder->add(4); // изменение для 7 версии не в этой ноде
  listOrder->add(6); // изменение для 8 версии не в этой ноде
  ASSERT_EQ(listNode.Find(3), 12);
  ASSERT_EQ(listNode.Find(4), 13);
  ASSERT_EQ(listNode.Find(7), 13);
  ASSERT_EQ(listNode.Find(8), 15);

  ListNode listNode1(3, 10, nullptr, nullptr, CmpByListVersion(listOrder));
  listNode1.Add(7, 11);
  ASSERT_EQ(listNode1.Find(4), 10);
}

TEST(PersistentListTests, FindList) {
  const PersistentList test({1, 2, 3, 4});
  ASSERT_EQ(test.Find(0), 1);
  ASSERT_EQ(test.Find(1), 2);
  ASSERT_EQ(test.Find(2), 3);
  ASSERT_EQ(test.Find(3), 4);
}

TEST(PersistentListTests, SetList) {
  PersistentList v1({1, 2, 3, 4});
  auto v2 = v1.Set(0, -1);
  auto v3 = v1.Set(1, -2);
  auto v4 = v2.Set(2, -3);
  //                v1 {1, 2, 3, 4}
  //                   /           \
  //         v2 {-1, 2, 3, 4}     v3 {1, -2, 3, 4}
  //                 /
  //    v4 {-1, 2, -3, 4}
  ASSERT_EQ(v2.Find(0), -1);
  ASSERT_EQ(v3.Find(0), 1);
  ASSERT_EQ(v3.Find(1), -2);
  ASSERT_EQ(v4.Find(2), -3);
  ASSERT_EQ(v4.Find(0), -1);
}

TEST(PersistentListTests, EraseList) {
  PersistentList v1({1, 2, 3, 4});
  auto v2 = v1.Erase(1);
  auto v3 = v1.Erase(2);
  auto v4 = v2.Erase(2);
  auto v5 = v4.Erase(0);
  //            v1 {1, 2, 3, 4}
  //             /           \
  //         v2 {1, 3, 4}     v3 {1, 2, 4}
  //          /
  //    v4 {1, 3}
  //        /
  //    v5 {3}
  ASSERT_EQ(v2.Find(0), 1);
  ASSERT_EQ(v2.Find(1), 3);
  ASSERT_EQ(v3.Find(0), 1);
  ASSERT_EQ(v3.Find(1), 2);
  ASSERT_EQ(v3.Find(2), 4);
  ASSERT_EQ(v4.Find(0), 1);
  ASSERT_EQ(v4.Find(1), 3);
  ASSERT_ANY_THROW(v4.Find(2), 4);
  ASSERT_EQ(v5.Find(0), 3);
}

// сделать push_back
TEST(PersistentListTests, InsertList) {
  PersistentList v1({1, 2, 3, 4});
  auto v2 = v1.Insert(1, 5);
  auto v3 = v1.Insert(1, 6);
  auto v4 = v2.Insert(1, 7);
  auto v5 = v4.Insert(0, 8);
  //                       v1 {1, 2, 3, 4}
  //                       /           \
  //         v2 {1, 5, 2, 3, 4}     v3 {1, 6, 2, 3, 4}
  //                     /               \
  //    v4 {1, 7, 5, 2, 3, 4}       v6 {1, 6, 2, 3, 4, 9}
  //                   /
  //    v5 {8, 1, 7, 5, 2, 3, 4}
  ASSERT_EQ(v2.Find(0), 1);
  ASSERT_EQ(v2.Find(1), 5);
  ASSERT_EQ(v2.Find(2), 2);
  ASSERT_EQ(v2.Find(3), 3);
  ASSERT_EQ(v2.Find(4), 4);
  ASSERT_EQ(v3.Find(0), 1);
  ASSERT_EQ(v3.Find(1), 6);
  ASSERT_EQ(v3.Find(2), 2);
  ASSERT_EQ(v3.Find(3), 3);
  ASSERT_EQ(v3.Find(4), 4);
  ASSERT_EQ(v4.Find(0), 1);
  ASSERT_EQ(v4.Find(1), 7);
  ASSERT_EQ(v4.Find(2), 5);
  ASSERT_EQ(v4.Find(3), 2);
  ASSERT_EQ(v4.Find(4), 3);
  ASSERT_EQ(v4.Find(5), 4);
  ASSERT_EQ(v5.Find(0), 8);
  ASSERT_EQ(v5.Find(1), 1);
  ASSERT_EQ(v5.Find(2), 7);
  ASSERT_EQ(v5.Find(3), 5);
  ASSERT_EQ(v5.Find(4), 2);
  ASSERT_EQ(v5.Find(5), 3);
  ASSERT_EQ(v5.Find(6), 4);
}
