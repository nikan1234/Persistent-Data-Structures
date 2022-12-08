#ifndef ARRAY_UTILS_H
#define ARRAY_UTILS_H

#include <Common/ContractExceptions.h>

#include <type_traits>
#include <vector>

namespace Util {

/// Creates new container with particular value inserted.
template <class ContainerValue, class TargetValue,
          class = std::enable_if_t<std::is_convertible_v<TargetValue, ContainerValue>>>
[[nodiscard]] auto vectorInserted(const std::vector<ContainerValue> &source,
                                  const typename std::vector<ContainerValue>::const_iterator where,
                                  TargetValue &&value) {
  std::vector<ContainerValue> destination;
  destination.reserve(std::size(source) + 1u);

  auto inserter = std::back_inserter(destination);
  inserter = std::copy(std::begin(source), where, inserter);
  *inserter++ = std::forward<TargetValue>(value);
  std::copy(where, std::end(source), inserter);
  return destination;
}

/// Creates new container with value replaced
template <class ContainerValue, class TargetValue,
          class = std::enable_if_t<std::is_convertible_v<TargetValue, ContainerValue>>>
[[nodiscard]] auto vectorReplaced(const std::vector<ContainerValue> &source,
                                  const typename std::vector<ContainerValue>::const_iterator where,
                                  TargetValue &&value) {
  CONTRACT_EXPECT(where < source.end());

  std::vector<ContainerValue> destination;
  destination.reserve(std::size(source));

  auto inserter = std::back_inserter(destination);
  inserter = std::copy(std::begin(source), where, inserter);
  *inserter++ = std::forward<TargetValue>(value);
  std::copy(std::next(where), std::end(source), inserter);
  return destination;
}

/// Creates new container with value erased
template <class ContainerValue>
[[nodiscard]] auto vectorErased(const std::vector<ContainerValue> &source,
                                const typename std::vector<ContainerValue>::const_iterator where) {
  if (std::empty(source) && where == std::end(source))
    return source;

  std::vector<ContainerValue> destination;
  destination.reserve(std::size(source) - 1);

  std::copy(source.begin(), where, std::back_inserter(destination));
  std::copy(std::next(where), source.end(), std::back_inserter(destination));
  return destination;
}

} // namespace Util

#endif
