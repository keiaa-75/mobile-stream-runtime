cmake_minimum_required(VERSION 3.16)
project(msr-engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

get_filename_component(MSR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Static llama.cpp built with -DCMAKE_POSITION_INDEPENDENT_CODE=ON; linked into
# libmsr_engine.so so the weak hooks resolve to the provider in the same unit.
set(MSR_LLAMA_STATIC_DIR "${MSR_ROOT}/build-static" CACHE PATH "static llama.cpp build tree")
set(_llama_libs
  "${MSR_LLAMA_STATIC_DIR}/src/libllama.a"
  "${MSR_LLAMA_STATIC_DIR}/ggml/src/libggml.a"
  "${MSR_LLAMA_STATIC_DIR}/ggml/src/libggml-cpu.a"
  "${MSR_LLAMA_STATIC_DIR}/ggml/src/libggml-base.a")
foreach(_lib IN LISTS _llama_libs)
  if(NOT EXISTS "${_lib}")
    message(FATAL_ERROR "missing ${_lib}; build static llama.cpp first (docs/FRAMEWORK.md)")
  endif()
endforeach()

file(GLOB MSR_CORE_SOURCES "${MSR_ROOT}/native/src/*/*.cpp")

add_library(msr_engine SHARED
  ${MSR_CORE_SOURCES}
  "${MSR_ROOT}/native/tools/msr_external_preload.cpp"
)
target_compile_definitions(msr_engine PRIVATE MSR_BUILDING_LIBRARY)
target_include_directories(msr_engine
  PUBLIC
    $<BUILD_INTERFACE:${MSR_ROOT}/native/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    "${MSR_ROOT}/third_party/llama.cpp/include"
    "${MSR_ROOT}/third_party/llama.cpp/ggml/include"
)
# Only MSR_API symbols are exported; provider hooks stay internal.
if(NOT MSVC)
  target_compile_options(msr_engine PRIVATE -fvisibility=hidden)
endif()
target_link_libraries(msr_engine PRIVATE
  ${_llama_libs}
  OpenSSL::Crypto
  Threads::Threads
  ${CMAKE_DL_LIBS}
  m
)
set_target_properties(msr_engine PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION 0
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

add_executable(msr-cli tools/msr_cli.cpp)
target_link_libraries(msr-cli PRIVATE msr_engine)
set_target_properties(msr-cli PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

enable_testing()
# Needs the reference model; skip on build hosts that do not have it.
if(EXISTS "/storage/emulated/0/Download/gemma/medgemma-1.5-4b-it.Q4_K_M.gguf")
  add_test(NAME framework_cli
           COMMAND msr-cli
                   /storage/emulated/0/Download/gemma/medgemma-1.5-4b-it.Q4_K_M.gguf
                   "The capital of France is" 8 1024 4 4 1)
endif()

install(TARGETS msr_engine EXPORT msrTargets
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib)
install(TARGETS msr-cli RUNTIME DESTINATION bin)
install(EXPORT msrTargets NAMESPACE msr:: DESTINATION lib/cmake/msr)
install(FILES cmake/msrConfig.cmake DESTINATION lib/cmake/msr)
install(DIRECTORY "${MSR_ROOT}/native/include/msr" DESTINATION include
        FILES_MATCHING PATTERN "*.h")
