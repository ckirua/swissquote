add_library(swissquote_lib STATIC
    codec/encode.cpp
    codec/decode.cpp
    codec/validate.cpp
    ipc/spsc_ring.cpp
    ipc/unix_pub.cpp
    ipc/unix_sub.cpp
    config/config.cpp
    net/http_client.cpp
    ingest/row_buffer.cpp
    clickhouse/client.cpp
    util/clock.cpp
    util/logging.cpp
    util/weekend_pause.cpp
)

target_include_directories(swissquote_lib
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_BINARY_DIR}/generated"
    PRIVATE
        "${CMAKE_SOURCE_DIR}/../sbe/generated"
        "${CMAKE_SOURCE_DIR}/../third_party/simdjson"
)

target_sources(swissquote_lib PRIVATE
    "${CMAKE_SOURCE_DIR}/../third_party/simdjson/simdjson.cpp"
)

target_compile_definitions(swissquote_lib PRIVATE SIMDJSON_EXCEPTIONS=0)

target_compile_options(swissquote_lib PRIVATE
    -O3
    -march=native
    -Wno-unused-function
    -Wno-unused-variable
)

target_link_libraries(swissquote_lib
    PUBLIC
        spdlog::spdlog
    PRIVATE
        tomlplusplus::tomlplusplus
)

target_include_directories(swissquote_lib SYSTEM PRIVATE
    "${httplib_SOURCE_DIR}"
)

if(OpenSSL_FOUND)
    target_link_libraries(swissquote_lib PUBLIC OpenSSL::SSL OpenSSL::Crypto)
endif()


add_library(swissquote::lib ALIAS swissquote_lib)
