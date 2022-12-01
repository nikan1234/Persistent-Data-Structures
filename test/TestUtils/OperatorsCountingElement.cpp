#include "OperatorsCountingElement.h"

namespace UnitTests {

OperatorsCountingElement::OperatorsCountingElement() { ++default_constructor_calls; }
OperatorsCountingElement::~OperatorsCountingElement() { ++destructor_calls; }

OperatorsCountingElement::OperatorsCountingElement(const OperatorsCountingElement &) {
  ++copy_constructor_calls;
}

OperatorsCountingElement::OperatorsCountingElement(OperatorsCountingElement &&) noexcept {
  ++move_constructor_calls;
}

OperatorsCountingElement &OperatorsCountingElement::operator=(const OperatorsCountingElement &) {
  ++copy_operator_calls;
  return *this;
}

OperatorsCountingElement &
OperatorsCountingElement::operator=(OperatorsCountingElement &&) noexcept {
  ++move_operator_calls;
  return *this;
}

int OperatorsCountingElement::defaultConstructorCalls() { return default_constructor_calls; }
int OperatorsCountingElement::copyConstructorCalls() { return copy_constructor_calls; }
int OperatorsCountingElement::moveConstructorCalls() { return move_constructor_calls; }
int OperatorsCountingElement::copyOperatorCalls() { return copy_operator_calls; }
int OperatorsCountingElement::moveOperatorCalls() { return move_operator_calls; }
int OperatorsCountingElement::destructorCalls() { return destructor_calls; }

void OperatorsCountingElement::reset() {
  default_constructor_calls = 0;
  copy_constructor_calls = 0;
  move_constructor_calls = 0;
  copy_operator_calls = 0;
  move_operator_calls = 0;
  destructor_calls = 0;
}
} // namespace UnitTests
