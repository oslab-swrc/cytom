#set(SRCS
#  "sample_binary.cc")

SET(SOURCES_RB_TEST
    rb-test.cc
    )

#add_executable(sample_fb ${SRCS})
#target_link_libraries(sample_fb PRIVATE flatbuffer_schemas)

add_executable(rb-test ${SOURCES_RB_TEST})
target_link_libraries(rb-test PRIVATE ring-buffer util flatbuffer_schemas)
