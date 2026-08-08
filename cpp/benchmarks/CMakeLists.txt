add_executable(swissquote_benchmarks
    benchmark_pipeline.cpp
)

target_link_libraries(swissquote_benchmarks
    PRIVATE
        swissquote_lib
        Catch2::Catch2WithMain
)

target_include_directories(swissquote_benchmarks PRIVATE
    "${CMAKE_SOURCE_DIR}/tests"
)
