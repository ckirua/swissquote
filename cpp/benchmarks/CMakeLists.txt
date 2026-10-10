if(SWISSQUOTE_BUILD_BENCHMARKS)
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
endif()

if(SWISSQUOTE_BUILD_CODEC_BENCHMARK)
    add_executable(swissquote_codec_benchmark
        benchmark_codec.cpp
    )
    target_link_libraries(swissquote_codec_benchmark PRIVATE swissquote_lib)
    target_include_directories(swissquote_codec_benchmark PRIVATE
        "${CMAKE_SOURCE_DIR}/tests"
    )

    add_executable(swissquote_codec_allocation_regression
        allocation_regression.cpp
    )
    target_link_libraries(swissquote_codec_allocation_regression PRIVATE swissquote_lib)
    target_include_directories(swissquote_codec_allocation_regression PRIVATE
        "${CMAKE_SOURCE_DIR}/tests"
    )
endif()
