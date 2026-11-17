cmake_minimum_required(VERSION 3.16)
project(msr_jni LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

get_filename_component(MSR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

# JNI headers: JAVA_HOME if set, otherwise the usual JDK locations (Termux
# keeps several JDK versions under $PREFIX/lib/jvm).
if(DEFINED ENV{JAVA_HOME})
  set(_jni_hints "$ENV{JAVA_HOME}/include")
else()
  file(GLOB _jni_hints
       /data/data/com.termux/files/usr/lib/jvm/java-*/include
       /usr/lib/jvm/*/include
       /Library/Java/JavaVirtualMachines/*/Contents/Home/include)
endif()
find_path(JNI_INCLUDE_DIR jni.h HINTS ${_jni_hints})
find_path(JNI_MD_INCLUDE_DIR jni_md.h HINTS ${JNI_INCLUDE_DIR}/linux
          ${JNI_INCLUDE_DIR}/darwin ${JNI_INCLUDE_DIR}/win32)
if(NOT JNI_INCLUDE_DIR OR NOT JNI_MD_INCLUDE_DIR)
  message(FATAL_ERROR "jni.h / jni_md.h not found; set JAVA_HOME or JNI_INCLUDE_DIR")
endif()

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

file(GLOB MSR_CORE_SOURCES "${MSR_ROOT}/native/src/*/*.cpp")

# The provider is compiled into the JNI library so its strong hook definitions
# satisfy llama.cpp's weak hook declarations in the same static link.
add_library(msr_jni SHARED
  msr_jni.cpp
  ${MSR_ROOT}/native/tools/msr_external_preload.cpp
  ${MSR_CORE_SOURCES}
)
target_include_directories(msr_jni PRIVATE
  ${JNI_INCLUDE_DIR} ${JNI_MD_INCLUDE_DIR}
  ${MSR_ROOT}/native/include
  ${MSR_ROOT}/third_party/llama.cpp/include
  ${MSR_ROOT}/third_party/llama.cpp/ggml/include
)

# Static llama/ggml (built with BUILD_SHARED_LIBS=OFF). Link order matters:
# llama -> ggml -> ggml-cpu -> ggml-base.
set(MSR_STATIC_DIR "${MSR_ROOT}/build-static" CACHE PATH "static llama.cpp build tree")
set(_llama_libs
  "${MSR_STATIC_DIR}/src/libllama.a"
  "${MSR_STATIC_DIR}/ggml/src/libggml.a"
  "${MSR_STATIC_DIR}/ggml/src/libggml-cpu.a"
  "${MSR_STATIC_DIR}/ggml/src/libggml-base.a")
foreach(_lib IN LISTS _llama_libs)
  if(NOT EXISTS "${_lib}")
    message(FATAL_ERROR "missing ${_lib}; build the static llama.cpp first (docs/native-api.md)")
  endif()
endforeach()

target_link_libraries(msr_jni PRIVATE
  ${_llama_libs}
  OpenSSL::Crypto
  Threads::Threads
  ${CMAKE_DL_LIBS}
  m
)
set_target_properties(msr_jni PROPERTIES
  OUTPUT_NAME msr_jni
  LIBRARY_OUTPUT_DIRECTORY "${MSR_ROOT}/native/build/jni"
)
