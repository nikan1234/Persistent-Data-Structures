#ifndef OPERATORS_COUNTING_ELEMENT_H
#define OPERATORS_COUNTING_ELEMENT_H

namespace UnitTests {
class OperatorsCountingElement {

  static inline int default_constructor_calls = 0;
  static inline int copy_constructor_calls = 0;
  static inline int move_constructor_calls = 0;
  static inline int copy_operator_calls = 0;
  static inline int move_operator_calls = 0;
  static inline int destructor_calls = 0;

public:
  OperatorsCountingElement();
  ~OperatorsCountingElement();
  OperatorsCountingElement(const OperatorsCountingElement &);
  OperatorsCountingElement(OperatorsCountingElement &&) noexcept;
  OperatorsCountingElement &operator=(const OperatorsCountingElement &);
  OperatorsCountingElement &operator=(OperatorsCountingElement &&) noexcept;

  [[nodiscard]] static int defaultConstructorCalls();
  [[nodiscard]] static int copyConstructorCalls();
  [[nodiscard]] static int moveConstructorCalls();
  [[nodiscard]] static int copyOperatorCalls();
  [[nodiscard]] static int moveOperatorCalls();
  [[nodiscard]] static int destructorCalls();

  static void reset();
};
} // namespace UnitTests

#endif
