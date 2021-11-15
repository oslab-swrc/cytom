set(SOURCES_SORT_RESULTS
    main_sort_results.cc
    )

set(SOURCES_RESULTS_DIFF
    main_results_diff.cc
    )

set(SOURCES_REWRITE_IDS
    main-rewrite-ids.cc
    )

add_executable(sort_results ${SOURCES_SORT_RESULTS})
add_executable(results_diff ${SOURCES_RESULTS_DIFF})
add_executable(rewrite_ids ${SOURCES_REWRITE_IDS})

target_link_libraries(sort_results PUBLIC util perf-event)
target_link_libraries(results_diff PUBLIC util perf-event)
target_link_libraries(rewrite_ids PUBLIC util perf-event)
