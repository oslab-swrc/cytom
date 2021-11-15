# find_package("Flatbuffers")
find_package("FlatBuffers")
include("BuildFlatBuffers")

set(FB_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/schema.fbs")

build_flatbuffers("${FB_SRCS}" "${CMAKE_CURRENT_SOURCE_DIR}"
  fb_generated_files "" "${CMAKE_CURRENT_BINARY_DIR}/include"
  "${CMAKE_CURRENT_BINARY_DIR}/binary"
  "${CMAKE_CURRENT_BINARY_DIR}/text")

add_library(flatbuffer_schemas INTERFACE)
add_dependencies(flatbuffer_schemas fb_generated_files)
target_include_directories(flatbuffer_schemas INTERFACE
  "${CMAKE_CURRENT_BINARY_DIR}/include" ${FLATBUFFERS_INCLUDE_DIRS})
# target_link_libraries(flatbuffer_schemas INTERFACE "flatbuffers::flatbuffers")
