include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
  )

file(GLOB CommonFiles "Common/*")
file(GLOB UndoFiles "Undo/*")

add_library(PersistentDataStructures STATIC 
  ${CommonFiles}  
  ${UndoFiles}
)

source_group("Common" FILES ${CommonFiles})
source_group("Undo" FILES ${UndoFiles})


add_executable(Application
    main.cpp
)

target_link_libraries(Application PersistentDataStructures)