include(GoogleTest)

add_executable(
  UnitTests
  RunAllTests.cpp
)

target_link_libraries(
  UnitTests
  GTest::gtest_main
)
