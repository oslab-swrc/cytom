set(SOURCES
    hilbert.cc
    column_first.cc
    row_first.cc
    traversal.cc)

add_library(traversal STATIC ${SOURCES})
target_include_directories(traversal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(traversal PUBLIC util)
