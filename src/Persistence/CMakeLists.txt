file(GLOB PersistenceFiles 
  "*.h" 
  "*.cpp"
)

add_library(Persistence STATIC
  ${PersistenceFiles}
)

set_target_properties(Persistence PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(Persistence Common)
