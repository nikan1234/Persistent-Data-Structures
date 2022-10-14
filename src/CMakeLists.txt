file(GLOB CommonFiles "Common/*")
file(GLOB UndoFiles "Undo/*")
file(GLOB CollectionsFiles "Collections/*")

add_library(Persistence STATIC 
  ${CommonFiles}  
  ${UndoFiles}
  ${CollectionsFiles}
)

source_group("Common" FILES ${CommonFiles})
source_group("Undo" FILES ${UndoFiles})
source_group("Collections" FILES ${CollectionsFiles})


add_executable(Application
    main.cpp
)

target_link_libraries(Application Persistence)