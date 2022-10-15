#ifndef SAFE_DEREF_H
#define SAFE_DEREF_H

#include <Common/ContractExceptions.h>

namespace Detail {
template <class T> [[nodiscard]] decltype(auto) safeDerefHelper(T &&value) {
  CONTRACT_EXPECT(value);
  return *value;
}
} // namespace Detail

#define SAFE_DEREF(object) Detail::safeDerefHelper(object)

#endif
