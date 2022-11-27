#ifndef SAFE_DEREF_H
#define SAFE_DEREF_H

#include <Common/ContractExceptions.h>

namespace Exceptions::Detail {
template <class T> [[nodiscard]] decltype(auto) safeDerefHelper(T &&value) {
  CONTRACT_EXPECT(value);
  return *value;
}
} // namespace Exceptions::Detail

#define SAFE_DEREF(object) Exceptions::Detail::safeDerefHelper(object)

#endif
