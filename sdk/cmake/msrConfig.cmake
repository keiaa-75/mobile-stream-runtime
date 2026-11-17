# libmsr_engine embeds llama.cpp, the provider and the core statically, so the
# only thing an installed consumer needs is this exported target.
include("${CMAKE_CURRENT_LIST_DIR}/msrTargets.cmake")
if(NOT TARGET msr::engine AND TARGET msr::msr_engine)
  add_library(msr::engine ALIAS msr::msr_engine)
endif()
