#ifndef CONTRACT_EXCEPTIONS_H
#define CONTRACT_EXCEPTIONS_H

#include <stdexcept>

namespace Exceptions {
class PreConditionFailure final : public std::runtime_error {
  using Base = std::runtime_error;

public:
  explicit PreConditionFailure(const std::string &msg) : Base(msg) {}
};

class AssertionFailure final : public std::runtime_error {
  using Base = std::runtime_error;

public:
  explicit AssertionFailure(const std::string &msg) : Base(msg) {}
};

class PostConditionFailure final : public std::runtime_error {
  using Base = std::runtime_error;

public:
  explicit PostConditionFailure(const std::string &msg) : Base(msg) {}
};

namespace Detail {
[[nodiscard]] std::string createDiagnosticMessage(const char *expression, const char *file,
                                                  int line);
}

#define CONTRACT_CHECK_INTERNAL(Expectation, Expr)                                                 \
  do {                                                                                             \
    if (!(Expr))                                                                                   \
      std::throw_with_nested(                                                                      \
          Expectation(Exceptions::Detail::createDiagnosticMessage(#Expr, __FILE__, __LINE__)));    \
  } while (false)

#define CONTRACT_EXPECT(Expr) CONTRACT_CHECK_INTERNAL(Exceptions::PreConditionFailure, Expr)
#define CONTRACT_ASSERT(Expr) CONTRACT_CHECK_INTERNAL(Exceptions::AssertionFailure, Expr)
#define CONTRACT_ENSURE(Expr) CONTRACT_CHECK_INTERNAL(Exceptions::PostConditionFailure, Expr)

#define SAFE_DEREF(object) *object

} // namespace Exceptions

#endif
