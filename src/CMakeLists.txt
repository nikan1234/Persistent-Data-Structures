file(GLOB CommonFiles "Common/*.h" "Common/*.cpp")
file(GLOB UndoFiles "Undo/*.h")
file(GLOB CollectionsFiles "Collections/*.h")

add_library(Persistence STATIC 
  ${CommonFiles}  
  ${UndoFiles}
  ${CollectionsFiles}
)

set_target_properties(Persistence PROPERTIES LINKER_LANGUAGE CXX)

source_group("Common" FILES ${CommonFiles})
source_group("Undo" FILES ${UndoFiles})
source_group("Collections" FILES ${CollectionsFiles})


add_executable(Application
    main.cpp
)

target_link_libraries(Application Persistence)