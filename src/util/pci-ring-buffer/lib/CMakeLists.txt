set(SOURCES
    ring_buffer.c
    ring_buffer_porting.c
    )

set_source_files_properties(ring_buffer.c PROPERTIES LANGUAGE C)
set_source_files_properties(ring_buffer_porting.c PROPERTIES LANGUAGE C)
#set(CMAKE_C_FLAGS_DEBUG "-DRING_BUFFER_TRACE_EVENT")

find_package(Threads)
add_library(pci_ring_buffer STATIC ${SOURCES})

target_link_libraries(pci_ring_buffer PUBLIC rt)
