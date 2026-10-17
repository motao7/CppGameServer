#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <latch>
#include <mutex>
#include <numeric>
#include <shared_mutex>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>
using namespace std;

constexpr int RUNS = 7;

uint64_t g_target = 0; // 目标运行数量
uint64_t g_counter = 0;
std::atomic<uint64_t> g_atomicCounter{0};
std::mutex g_mutex;
std::shared_mutex g_sharedMutex;

uint64_t OperationsForThread(size_t thread_index, size_t thread_count)
{
    const uint64_t base= g_target / thread_count;
    const uint64_t remainder = g_target % thread_count;
    return base + (thread_index < remainder ? 1 : 0);
}

template<typename Operation>
chrono::nanoseconds BenchmarkThreads(size_t thread_count, Operation&& operation)
{
    latch ready(static_cast<ptrdiff_t>(thread_count));
    latch start_gate(1);
    latch finished(static_cast<ptrdiff_t>(thread_count));

    vector<thread> threads;
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
    const auto begin = chrono::steady_clock::now();
    start_gate.count_down();
    finished.wait();
    const auto end = chrono::steady_clock::now();

    for(thread& thread : threads)
    {
        thread.join();
    }

    return chrono::duration_cast<chrono::nanoseconds>(end - begin);
}

chrono::nanoseconds BenchmarkMutex(size_t thread_count)
{
    g_counter = 0;
    const chrono::nanoseconds elapsed = BenchmarkThreads(thread_count,[](size_t, uint64_t operations)
        {
            for(uint64_t index = 0; index < operations; ++index)
            {
                lock_guard<mutex> lock(g_mutex);
                ++g_counter;
            }
        }
    );

    return elapsed;
}

chrono::nanoseconds BenchmarkSharedMutex(size_t thread_count)
{
    g_counter = 0;
    const chrono::nanoseconds elapsed = BenchmarkThreads(thread_count,[](size_t, uint64_t operations)
        {
            for(uint64_t index = 0; index < operations; ++index)
            {
                lock_guard<shared_mutex> lock(g_sharedMutex);
                ++g_counter;
            }
        }
    );

    return elapsed;
}

chrono::nanoseconds BenchmarkAutomic(size_t thread_count)
{
    g_atomicCounter.store(0, memory_order_relaxed);
    const chrono::nanoseconds elapsed = BenchmarkThreads(thread_count,[](size_t, uint64_t operations)
        {
            for(uint64_t index = 0; index < operations; ++index)
            {
                g_atomicCounter.fetch_add(1, memory_order_relaxed);
            }
        }
    );

    return elapsed;
}

chrono::nanoseconds BenchmarkAutomicCas(size_t thread_count)
{
    g_atomicCounter.store(0, memory_order_relaxed);
    const chrono::nanoseconds elapsed = BenchmarkThreads(thread_count,[](size_t, uint64_t operations)
        {
            for(uint64_t index = 0; index < operations; ++index)
            {
                uint64_t expected = g_atomicCounter.load(memory_order_relaxed);
                while(!g_atomicCounter.compare_exchange_weak(expected, expected + 1, memory_order_relaxed, memory_order_relaxed))
                {
                }
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

void PrintResult(string_view name, double total_nanoseconds)
{
    const double nanoseconds_per_operation = total_nanoseconds / static_cast<double>(g_target);
    cout << left << setw(14) << name << right
         << total_nanoseconds << "ns \ttotal, "
         << nanoseconds_per_operation << "ns/op\n";
}

int main(int args, char** argv)
{
    if(args != 2)
    {
        std::cerr << "Usage: ./Test_Lock count\n";
        return -1;
    }
    g_target = std::atoi(argv[1]);

    cout << std::fixed << std::setprecision(2);
    cout << "operations" << g_target << ", average of " << RUNS << "runs\n\n";
    
    const vector<size_t> thread_counts = {1, 2, 4, 8, 10};
    for(const size_t thread_count : thread_counts)
    {
        cout << "threads: " << thread_count << "\n";
        PrintResult("Mutex",            Run([&](){ return BenchmarkMutex(thread_count);}));
        PrintResult("SharedMutex",      Run([&](){ return BenchmarkSharedMutex(thread_count);}));
        PrintResult("Automic",          Run([&](){ return BenchmarkAutomic(thread_count);}));
        PrintResult("AutomicCas",       Run([&](){ return BenchmarkAutomicCas(thread_count);}));
        cout << endl;
    }

    return 0;
}