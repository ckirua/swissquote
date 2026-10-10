#include "fixtures.hpp"

#include <swissquote/codec/codec.hpp>

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <span>
#include <string_view>
#include <vector>

namespace {

std::atomic<bool> g_count_allocations{false};
std::atomic<std::size_t> g_allocations{0};

void* allocate(std::size_t size) {
    void* result = std::malloc(size == 0 ? 1 : size);
    if (result == nullptr) {
        throw std::bad_alloc();
    }
    if (g_count_allocations.load(std::memory_order_relaxed)) {
        g_allocations.fetch_add(1, std::memory_order_relaxed);
    }
    return result;
}

void* operator_new(std::size_t size) { return allocate(size); }
void operator_delete(void* pointer) noexcept { std::free(pointer); }

} // namespace

void* operator new(std::size_t size) { return operator_new(size); }
void* operator new[](std::size_t size) { return operator_new(size); }
void operator delete(void* pointer) noexcept { operator_delete(pointer); }
void operator delete[](void* pointer) noexcept { operator_delete(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { operator_delete(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { operator_delete(pointer); }

namespace {
using swissquote::codec::BboRowView;
using swissquote::codec::EncodeWorkspace;
using swissquote::codec::encode_bbo_frame;
using swissquote::codec::encode_bbo_frame_into;
using swissquote::codec::flatten_bbo_frame_views;
using swissquote::test::kSampleFetchTimeMs;
using swissquote::test::kSampleJson;
using swissquote::test::kSampleSymbol;

std::span<const std::byte> as_bytes(std::string_view value) {
    return {reinterpret_cast<const std::byte*>(value.data()), value.size()};
}

template <typename Operation>
std::size_t measured_allocations(Operation&& operation, int iterations = 32) {
    for (int i = 0; i < 8; ++i) {
        static_cast<void>(operation());
    }
    g_allocations.store(0, std::memory_order_relaxed);
    g_count_allocations.store(true, std::memory_order_release);
    for (int i = 0; i < iterations; ++i) {
        static_cast<void>(operation());
    }
    g_count_allocations.store(false, std::memory_order_release);
    return g_allocations.load(std::memory_order_relaxed);
}
} // namespace

int main() {
    EncodeWorkspace workspace;
    std::vector<BboRowView> rows;
    const auto frame_result = encode_bbo_frame(
        kSampleSymbol, as_bytes(kSampleJson), kSampleFetchTimeMs);
    if (!frame_result) {
        std::cerr << "allocation regression: initial encode failed\n";
        return EXIT_FAILURE;
    }

    const auto reusable_encode = [&] {
        return encode_bbo_frame_into(
            kSampleSymbol, as_bytes(kSampleJson), kSampleFetchTimeMs, workspace)
            ? workspace.output.size()
            : 0;
    };
    const auto reusable_decode = [&] {
        return flatten_bbo_frame_views(*frame_result, rows) ? rows.size() : 0;
    };
    const auto legacy_encode = [&] {
        return encode_bbo_frame(kSampleSymbol, as_bytes(kSampleJson), kSampleFetchTimeMs)
            .value_or(std::vector<std::byte>{})
            .size();
    };

    const auto encode_allocations = measured_allocations(reusable_encode);
    const auto decode_allocations = measured_allocations(reusable_decode);
    const auto legacy_allocations = measured_allocations(legacy_encode);
    std::cout << "reusable_encode_allocations=" << encode_allocations
              << " reusable_view_decode_allocations=" << decode_allocations
              << " legacy_encode_allocations=" << legacy_allocations << '\n';
    if (encode_allocations != 0 || decode_allocations != 0) {
        std::cerr << "allocation regression failed\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
