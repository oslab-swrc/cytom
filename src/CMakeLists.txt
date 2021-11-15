###
# project setting
project(evolving-graphs C CXX)
cmake_minimum_required(VERSION 3.12)

# - set general options

#set(COMMON_FLAGS  "-march=native -mtune=native -fno-rtti ${TARGET_OPT} ${COMP_DEP_OPT}")
set(COMMON_FLAGS "-fno-rtti ${TARGET_OPT} ${COMP_DEP_OPT}")
set(DEBUG_FLAGS "-O0 -g -fno-omit-frame-pointer")
set(RELEASE_FLAGS "-O3")

set(CMAKE_C_FLAGS "${COMMON_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG "${DEBUG_FLAGS}")
set(CMAKE_C_FLAGS_RELEASE "${RELEASE_FLAGS}")

set(CMAKE_CXX_FLAGS "${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "${DEBUG_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "${RELEASE_FLAGS}")

if ( CMAKE_COMPILER_IS_GNUCC )
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()
if ( MSVC )
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /W4")
endif()

set(CMAKE_CXX_STANDARD 14)

find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

# - set debug options
# enables printing debug-information
add_compile_definitions($<$<CONFIG:Debug>:EVOLVING_GRAPHS_DEBUG>)
add_compile_definitions($<$<CONFIG:Debug>:RING_BUFFER_DEBUG>)
add_compile_definitions(MOSAIC_HOST_ONLY)
# add_definitions(-DRING_BUFFER_TRACE_FINGERPRINT)
# add_definitions(-DRING_BUFFER_TRACE_EVENT)

# - enable for semi-production-level testing
# add_definitions(-DRING_BUFFER_CONF_NO_MMAP)
# add_definitions(-DRING_BUFFER_CONF_NO_DOUBLE_MMAP)

# add_definitions(-DRING_BUFFER_TRACE_EVENT)
# add_definitions(-DPROF_TILE_INFO)
###
# - where to find cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake_modules")

set(EVOLVING_GRAPH_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/util/pci-ring-buffer/include)

include_directories(${EVOLVING_GRAPH_INCLUDE_DIR})

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

add_subdirectory(flatbuffer)
add_subdirectory(fb-test)
add_subdirectory(lib)
add_subdirectory(util)

