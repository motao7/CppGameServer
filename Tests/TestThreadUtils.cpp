#include <chrono>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

#if defined(__linux__)
#include <unistd.h>
#endif

#include <CAS/Utils/ThreadUtils.h>

namespace
{

constexpr auto kLongWait  = std::chrono::seconds(30);
constexpr auto kShortWait = std::chrono::seconds(10);

void Wait(std::chrono::seconds duration)
{
    std::this_thread::sleep_for(duration);
}

void PrintThreadName(const char* tag)
{
    const std::string_view name = CAS::GetThreadName();
    std::cout << tag
              << " tid=" << CAS::GetThreadId()
              << " name=\"" << name << "\""
              << " (bytes=" << name.size() << ")\n"
              << std::flush;
}

} // namespace

int main()
{
#if defined(__linux__)
    std::cout << "pid = " << ::getpid()
              << "  → 另开终端观察：top -H -p " << ::getpid()
              << "  或  ps -T -p " << ::getpid() << "\n" << std::flush;
#endif

    // 主线程：初始名通常是进程名
    PrintThreadName("1. main");
    Wait(kLongWait);

    CAS::SetThreadName("main_thread");
    PrintThreadName("2. main");
    Wait(kShortWait);

    // 5 个汉字 × 3 字节 = 15 字节，刚好卡在 Linux 的 15 字节上限
    CAS::SetThreadName("花间客真帅");
    PrintThreadName("3. main");
    Wait(kShortWait);

    // 8 个汉字 = 24 字节，C++ 侧保留全名，内核侧会被截断
    CAS::SetThreadName("磐石磐石坚如磐石");
    PrintThreadName("4. main");
    Wait(kShortWait);

    std::thread worker([] {
        // 新线程的初始名继承自创建它的线程（此处即主线程被截断后的名字）
        PrintThreadName("5. worker");
        Wait(kShortWait);

        // 20 字节 ASCII，内核侧会被截断为 15 字节
        CAS::SetThreadName("son-thread-long-name");
        PrintThreadName("6. worker");
        Wait(kShortWait);

#if defined(NDEBUG)
        // 空串 = 重置为系统原生线程名（此时原生名已是截断后的 son-thread-long）
        CAS::SetThreadName("");
        PrintThreadName("7. worker reset");
        Wait(kShortWait);
#else
        std::cout << "7. worker: SetThreadName(\"\") 在 Debug 下会触发断言，"
                     "请用 Release 构建验证重置行为\n" << std::flush;
#endif
    });
    worker.join();

    std::cout << "done\n";
    return 0;
}
