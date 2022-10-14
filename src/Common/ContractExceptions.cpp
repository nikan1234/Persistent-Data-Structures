#include "ContractExceptions.h"

#include <sstream>

namespace Exceptions {
namespace Detail {

[[nodiscard]] std::string createDiagnosticMessage(const char *expression, const char *file,
                                                  const int line) {
  std::ostringstream ss;
  ss << file << ':' << line << ": condition failed: " << expression;
  return ss.str();
}

} // namespace Detail
} // namespace Exceptions
