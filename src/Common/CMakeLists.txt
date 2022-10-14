file(GLOB CommonFiles 
  "*.h" 
  "*.cpp"
)

add_library(Common STATIC
  ${CommonFiles}
)

set_target_properties(Common PROPERTIES LINKER_LANGUAGE CXX)
