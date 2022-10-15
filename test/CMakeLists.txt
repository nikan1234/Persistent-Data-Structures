include(GoogleTest)

file(GLOB UndoTests "Undo/*")
file(GLOB CollectionsTests "Collections/*")

add_executable(
  UnitTests
  RunAllTests.cpp
  ${UndoTests}
  ${CollectionsTests}
)

source_group("Undo" FILES ${UndoTests})
source_group("Collections" FILES ${CollectionsTests})

target_link_libraries(
  UnitTests
  Persistence
  GTest::gtest_main
  GTest::gmock_main
)
