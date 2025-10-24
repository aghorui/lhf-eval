cmake_minimum_required(VERSION 3.23)

project(example-project LANGUAGES C CXX)

include_guard(GLOBAL)

set(MINIMUM_GCC_VERSION 11.0)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SRC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(INCLUDE_ROOT "${SRC_ROOT}/include")

# Configure and add LLVM-14 to the project.
find_package(LLVM 14.0 REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
llvm_map_components_to_libnames(llvm_libs ${LLVM_TARGETS_TO_BUILD} all)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

add_llvm_pass_plugin(FlattenConstExprPass "${SRC_ROOT}/FlattenConstExprPass.cpp")
