set(SOURCES
    util.cc
    runnable.cc
    config.cc)

find_package(Threads)

add_library(util STATIC ${SOURCES})
target_link_libraries(util PUBLIC flatbuffer_schemas ring-buffer ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
