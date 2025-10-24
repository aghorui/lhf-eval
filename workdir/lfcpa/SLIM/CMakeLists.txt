cmake_minimum_required(VERSION 3.5)
PROJECT(slim)

set(ENV{LLVM_DIR} /usr/lib/llvm-14/lib/cmake/llvm)

option(MemorySSAFlag "To use Memory SSA for creating different SSA versions of globals and address taken locals" OFF)
option(DiscardPointers "To discard instructions that contain or depend on pointer variables" OFF)

if (MemorySSAFlag)
    add_definitions(-DMemorySSAFlag=1)
else ()
    #add_definitions(-DMemorySSAFlag=0)
    remove_definitions(-DMemorySSAFlag=1)
endif()

if (DiscardPointers)
    add_definitions(-DDiscardPointers=1)
else ()
    #add_definitions(-DDiscardPointers=0)
    remove_definitions(-DDiscardPointers=1)
endif()

if (DiscardForSSA)
    add_definitions(-DDiscardForSSA=1)
else ()
    #add_definitions(-DDiscardPointers=0)
    remove_definitions(-DDiscardForSSA=1)
endif()

if (DISABLE_IGNORE_EFFECT)
    add_definitions(-DDISABLE_IGNORE_EFFECT=1)
else ()
    remove_definitions(-DDISABLE_IGNORE_EFFECT=1)
endif()

if (DEBUG)
    add_definitions(-DDEBUG=1)
else ()
    remove_definitions(-DDEBUG=1)
endif()

include_directories(include)

find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

#llvm_map_components_to_libnames(llvm_libs ${LLVM_TARGETS_TO_BUILD} all)

add_library(slim SHARED
    IR.cpp
    Instructions.cpp
    Operand.cpp
    SLIMDebug.cpp
)

target_link_libraries(slim LLVM)

# set_target_properties(slim PROPERTIES
#     COMPILE_FLAGS "-g -std=c++14 -fno-rtti -fPIC"
# )
include(GNUInstallDirs)
install(TARGETS slim LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY
    include/ DESTINATION /usr/local/include/slim)
add_library(slimPass SHARED Passes/GlobalInit.cpp)
add_library(mergeExit SHARED Passes/MergeExit.cpp)
