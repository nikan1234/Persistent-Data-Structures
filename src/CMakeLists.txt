cmake_minimum_required(VERSION 3.12)
project (Persistent-Data-Structures)

set(CMAKE_CXX_STANDARD 17)
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
  )

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Common)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Undo)

add_executable(PersistentDataStructures
    main.cpp
)

target_link_libraries(PersistentDataStructures Common)
# Uncomment if messesary
#target_link_libraries(PersistentDataStructures Undo)
