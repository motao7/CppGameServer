#include <iostream>
#include <chrono>
#include <ctime>
#include <string>
#include <vector>
#include <algorithm>

// 防止编译器把循环体优化掉
volatile long g_sink = 0;

void test_time(int times)
{
    const std::string format = "%Y-%m-%d %H:%M:%S";

    static thread_local time_t last_second = 0;
    static thread_local char cached_date_time[20] = {'\0'};

    auto t1 = std::chrono::steady_clock::now();

    // 基线：空循环
    for (int i = 0; i < times; ++i) {}
    auto t2 = std::chrono::steady_clock::now();

    // 方案 A：每次都 localtime_r + strftime
    for (int i = 0; i < times; ++i)
    {
        time_t now = time(0);
        struct tm tm;
        localtime_r(&now, &tm);
        char buf[64];
        std::size_t n = strftime(buf, sizeof(buf), format.c_str(), &tm);
        g_sink += buf[0] + static_cast<long>(n);   // 消费结果，防优化
    }
    auto t3 = std::chrono::steady_clock::now();

    // 方案 B：按秒缓存
    last_second = 0;   // 重置，避免跨调用残留影响
    for (int i = 0; i < times; ++i)
    {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        time_t current_second =
            std::chrono::duration_cast<std::chrono::seconds>(duration).count();

        if (current_second != last_second)
        {
            struct tm buf;
            localtime_r(&current_second, &buf);
            strftime(cached_date_time, sizeof(cached_date_time),
                     "%Y-%m-%d %H:%M:%S", &buf);
            last_second = current_second;
        }
        g_sink += cached_date_time[0];   // 消费结果，防优化
    }
    auto t4 = std::chrono::steady_clock::now();

    auto base   = t2 - t1;
    auto diff1  = t3 - t2 - base;
    auto diff2  = t4 - t3 - base;

    auto base_ns  = std::chrono::duration_cast<std::chrono::nanoseconds>(base).count();
    auto diff1_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(diff1).count();
    auto diff2_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(diff2).count();

    std::cout << "基础耗时: " << base_ns  << " ns\n";
    std::cout << "原耗时:   " << diff1_ns << " ns\n";
    std::cout << "现耗时:   " << diff2_ns << " ns\n";
    std::cout << "优化倍数: " << (double)diff1_ns / (double)diff2_ns << "\n";
}

int main()
{
    const int times = 1000000;   // 100 万次

    // 跑多轮取中位数，排除首次冷启动和噪声
    std::vector<double> ratios;
    for (int round = 0; round < 5; ++round)
    {
        std::cout << "===== 第 " << (round + 1) << " 轮 =====\n";
        test_time(times);
        std::cout << "\n";
    }

    // 若想统计倍数中位数，可把 test_time 的返回值改出来，
    // 这里为简洁只做多轮打印，观察数值稳定性即可。
    return 0;
}