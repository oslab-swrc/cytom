set(SOURCES
    perf-event-collector.cc
    perf-event-manager.cc
    perf-event-ringbuffer-sizes.cc
    perf-event-scoped.cc
    )

add_library(perf-event STATIC ${SOURCES})
target_include_directories(perf-event PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(perf-event PUBLIC util ring-buffer flatbuffer_schemas)

#target_link_libraries (perf-event rt pci_ring_buffer)
