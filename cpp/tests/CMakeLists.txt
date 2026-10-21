add_executable(swissquote_tests
    test_codec.cpp
    test_validate.cpp
    test_ipc.cpp
    test_weekend_pause.cpp
    test_ingest.cpp
)

target_link_libraries(swissquote_tests PRIVATE swissquote_lib Catch2::Catch2WithMain)
target_include_directories(swissquote_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

include(Catch)
catch_discover_tests(swissquote_tests)
