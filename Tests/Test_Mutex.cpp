#include "CAS/Mutex.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <latch>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

constexpr int RUNS=7;
uint64_t g_counter=0;
std::uint64_t g_target=0;

class Spinlock
{
    public:
        using Lock = std::lock_guard<Spinlock>;
        
        Spinlock() noexcept = default;
        ~Spinlock() noexcept = default;

        Spinlock(const Spinlock&) = delete;
        Spinlock& operator=(const Spinlock&) = delete;

        void lock() noexcept
        {
            while(m_mutex.test_and_set(std::memory_order_acquire))
            {
                while(m_mutex.test(std::memory_order_relaxed))
                {
                    cpu_relax();
                }
            }
        }

        [[nodiscard]] bool try_lock() noexcept
        {
            return !m_mutex.test_and_set(std::memory_order_acquire);
        }

        void unlock() noexcept
        {
            m_mutex.clear(std::memory_order_release);
        }

    private:
        std::atomic_flag m_mutex = ATOMIC_FLAG_INIT;
};

uint64_t OperationsForThread(size_t thread_index, size_t thread_count)
{
    const uint64_t base= g_target / thread_count;
    const uint64_t remainder = g_target % thread_count;
    return base + (thread_index < remainder ? 1 : 0);
}

template<typename Operation>
std::chrono::nanoseconds BenchmarkThreads(size_t thread_count, Operation&& operation)
{
    std::latch ready(static_cast<ptrdiff_t>(thread_count));
    std::latch start_gate(1);
    std::latch finished(static_cast<ptrdiff_t>(thread_count));

    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for(size_t index = 0; index < thread_count ; ++index)
    {
        threads.emplace_back([&, index]()
        {
            ready.count_down();
            start_gate.wait();
            operation(index, OperationsForThread(index, thread_count));
            finished.count_down();
        });
    }

    ready.wait();
    const auto begin = std::chrono::steady_clock::now();
    start_gate.count_down();
    finished.wait();
    const auto end = std::chrono::steady_clock::now();

    for(std::thread& thread : threads)
    {
        thread.join();
    }

    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
}

template<typename Mutex>
std::chrono::nanoseconds BenchmarkMutex(size_t thread_count)
{
    Mutex mutex;
    g_counter = 0;
    const std::chrono::nanoseconds elapsed = BenchmarkThreads(thread_count,[&mutex](size_t, uint64_t operations)
        {
            for(uint64_t index = 0; index < operations; ++index)
            {
                std::lock_guard<Mutex> lock(mutex);
                ++g_counter;
            }
        }
    );

    return elapsed;
}

template<typename Benchmark>
double Run(Benchmark&& benchmark)
{
    long double total_nanoseconds = 0.0L;
    for(int run = 0 ; run < RUNS ; ++run)
    {
        total_nanoseconds += static_cast<long double>(benchmark().count());
    }

    return static_cast<double>(total_nanoseconds / RUNS);
}

void PrintResult(std::string_view name, double total_nanoseconds)
{
    const double nanoseconds_per_operation = total_nanoseconds / static_cast<double>(g_target);
    std::cout << std::left << std::setw(40) << name << std::right
         << total_nanoseconds << "ns \ttotal, "
         << nanoseconds_per_operation << "ns/op\n";
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: ./test_mutex count\n";
        return EXIT_FAILURE;
    }

    g_target = std::stoull(argv[1]);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "operations: " << g_target
              << ", average of " << RUNS << " runs\n\n";

    const std::vector<std::size_t> thread_counts = {1, 2, 4, 8, 10};

    for (const std::size_t thread_count : thread_counts)
    {
        std::cout << "threads: " << thread_count << "\n";

        PrintResult("pthread_spinlock::Spinlock",
            Run([&]() { return BenchmarkMutex<CAS::Spinlock>(thread_count); }));

        PrintResult("atomic_flag::Spinlock",
            Run([&]() { return BenchmarkMutex<Spinlock>(thread_count); }));

        PrintResult("std::mutex",
            Run([&]() { return BenchmarkMutex<std::mutex>(thread_count); }));

        std::cout << '\n';
    }

    return EXIT_SUCCESS;
}