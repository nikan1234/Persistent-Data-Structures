include(GoogleTest)

file(GLOB TestUtils "TestUtils/*")
file(GLOB UndoTests "Undo/*")
file(GLOB CollectionsTests "Collections/*")


add_executable(
  UnitTests
  RunAllTests.cpp
  ${TestUtils}
  ${UndoTests}
  ${CollectionsTests}
)

target_include_directories(UnitTests PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/TestUtils)

source_group("TestUtils" FILES ${TestUtils})
source_group("Undo" FILES ${UndoTests})
source_group("Collections" FILES ${CollectionsTests})

target_link_libraries(
  UnitTests
  Persistence
  GTest::gtest_main
  GTest::gmock_main
)
