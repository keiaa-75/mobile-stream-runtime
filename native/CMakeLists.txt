cmake_minimum_required(VERSION 3.16)
project(msr_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_library(msr_core STATIC
  src/storage/model_file.cpp
  src/model/gguf.cpp
  src/model/integrity.cpp
  src/memory/memory_manager.cpp
  src/backend/backend_adapter.cpp
  src/streaming/layer_plan.cpp
  src/streaming/streaming_loader.cpp
  src/scheduler/async_reader.cpp
)
target_include_directories(msr_core PUBLIC include)
target_link_libraries(msr_core PUBLIC OpenSSL::Crypto Threads::Threads)
set_target_properties(msr_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(msr-external SHARED tools/msr_external_preload.cpp)
target_link_libraries(msr-external PRIVATE msr_core)

add_executable(model-inspect tools/model_inspect.cpp)
target_link_libraries(model-inspect PRIVATE msr_core)

add_executable(test-gguf tests/test_gguf.cpp)
target_link_libraries(test-gguf PRIVATE msr_core)

add_executable(test-streaming tests/test_streaming.cpp)
target_link_libraries(test-streaming PRIVATE msr_core)

add_executable(test-scheduler tests/test_scheduler.cpp)
target_link_libraries(test-scheduler PRIVATE msr_core)

enable_testing()
add_test(NAME gguf_fixtures
         COMMAND test-gguf ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures)
add_test(NAME streaming_budget
         COMMAND test-streaming
                 /storage/emulated/0/Download/gemma/medgemma-1.5-4b-it.Q4_K_M.gguf)
add_test(NAME scheduler_async COMMAND test-scheduler)
