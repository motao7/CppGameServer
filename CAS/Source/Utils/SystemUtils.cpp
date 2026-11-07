#include <CAS/Utils/SystemUtils.h>

#include <chrono>

#include <CAS/Macros.h>

namespace CAS
{

std::chrono::steady_clock::duration GetElapsedTime() noexcept
{
    // 函数内静态变量只在首次调用时初始化，且初始化是线程安全的（C++11 起的 magic static）
    static const auto START_TIME = std::chrono::steady_clock::now();
    return std::chrono::steady_clock::now() - START_TIME;
}

u64 GetFiberId() noexcept
{
    // 纤程调度器尚未接入：当前线程即最小执行单元，返回 0 表示"不在纤程中"。
    // 接入调度器后改为读取 thread_local 的纤程 ID，调用方与格式化输出均无需改动。
    return 0;
}

} // namespace CAS
