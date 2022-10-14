enable_testing()

add_executable(
  UnitTests
  RunAllTests.cpp
)

target_link_libraries(
  UnitTests
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(UnitTests)