#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include "Logger/Buffer.h"
#include "Logger/BufferConfig.h"

constexpr int RUNS = 7;
std::uint64_t g_target = 0;
std::atomic<std::uint64_t> BENCHMARK_SINK{0};

std::uint64_t Observe(std::string_view value) noexcept
{
    return static_cast<std::uint64_t>(value.size())
         + static_cast<unsigned char>(value.front())
         + static_cast<unsigned char>(value[value.size() / 2])
         + static_cast<unsigned char>(value.back());
}

std::uint64_t WriteWithSmallStreamBuffer(std::string_view message)
{
    CAS::Detail::InlineBuffer<CAS::Detail::LOG_MESSAGE_INLINE_CAPACITY> buffer;
    CAS::Detail::SmallStreamBuffer<CAS::Detail::LOG_MESSAGE_INLINE_CAPACITY> stream_buffer(buffer);

    std::ostream stream(&stream_buffer);
    stream << message;
    stream.flush();

    return Observe(buffer.view());
}

std::uint64_t WriteWithStringStream(std::string_view message)
{
    std::stringstream stream;
    stream << message;
    // std::stringstream has no .view(); use .str() and observe through it.
    return Observe(stream.str());
}

template<typename Operation>
std::chrono::nanoseconds BenchmarkWrites(Operation&& operation)
{
    std::uint64_t checksum = 0;
    const auto begin = std::chrono::steady_clock::now();
    for (std::uint64_t iter = 0; iter < g_target; ++iter)
    {
        checksum += operation();
    }
    const auto end = std::chrono::steady_clock::now();

    BENCHMARK_SINK.fetch_xor(checksum, std::memory_order_relaxed);
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
}

template<typename Benchmark>
double Run(Benchmark&& benchmark)
{
    long double total_nanoseconds = 0.0L;
    for (int run = 0; run < RUNS; ++run)
    {
        total_nanoseconds += static_cast<long double>(benchmark().count());
    }
    return static_cast<double>(total_nanoseconds / RUNS);
}

void PrintResult(std::string_view name, double total_nanoseconds)
{
    const double nanoseconds_per_operation = total_nanoseconds / static_cast<double>(g_target);
    std::cout << std::left << std::setw(20) << name << std::right
              << total_nanoseconds << " ns total, "
              << nanoseconds_per_operation << " ns/op\n";
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: ./test_log_buffer count\n";
        return 1;
    }
    g_target = std::stoull(argv[1]);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "operations: " << g_target << ", average of " << RUNS << " runs\n\n";

    constexpr std::array<std::size_t, 6> MESSAGE_SIZES = {50, 100, 200, 500, 1000, 2048};

    for (const std::size_t message_size : MESSAGE_SIZES) {
        const std::string message(message_size, 'x');

        const double small_stream_buffer_nanoseconds = Run([&]() {
            return BenchmarkWrites([&]() {
                return WriteWithSmallStreamBuffer(message);
            });
        });
        const double stringstream_nanoseconds = Run([&]() {
            return BenchmarkWrites([&]() {
                return WriteWithStringStream(message);
            });
        });

        std::cout << "message: " << message_size << " characters\n";
        PrintResult("SmallStreamBuffer", small_stream_buffer_nanoseconds);
        PrintResult("std::stringstream", stringstream_nanoseconds);
        std::cout << "speedup: "
                  << stringstream_nanoseconds / small_stream_buffer_nanoseconds
                  << "x\n\n";
    }
    return 0;
}