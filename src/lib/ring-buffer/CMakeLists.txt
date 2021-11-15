set(SOURCES
    ring-buffer.cc
    )

add_library(ring-buffer STATIC ${SOURCES})
target_link_libraries(ring-buffer PUBLIC pci_ring_buffer)
target_include_directories(ring-buffer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
