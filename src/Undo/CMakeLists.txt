file(GLOB UndoFiles 
  "*.h" 
  "*.cpp"
)

add_library(Undo STATIC
  ${UndoFiles}
)

set_target_properties(Undo PROPERTIES LINKER_LANGUAGE CXX)
