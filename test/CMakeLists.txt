include(GoogleTest)

file(GLOB CollectionsTests "Collections/*")

add_executable(
  UnitTests
  RunAllTests.cpp
  ${CollectionsTests}
)

source_group("Collections" FILES ${CollectionsTests})

target_link_libraries(
  UnitTests
  Persistence
  GTest::gtest_main
)
