#ifndef CAS_INCLUDE_CAS_UTILS_SYSTEM_UTILS_H
#define CAS_INCLUDE_CAS_UTILS_SYSTEM_UTILS_H

#include <chrono>

#include <CAS/Export.h>
#include <CAS/Macros.h>

namespace CAS
{

/**
 * @brief 获取进程启动至今经过的时间
 *
 * 使用 steady_clock（单调时钟），不受系统时间调整影响，适用于耗时统计与日志时间戳。
 * 起始点由首次调用时的静态初始化确定，因此首次调用之前的耗时不计入。
 */
[[nodiscard]] CAS_API std::chrono::steady_clock::duration GetElapsedTime() noexcept;

/**
 * @brief 获取当前纤程（协程）ID
 * @return 当前处于纤程中时返回纤程 ID；不在纤程中返回 0
 */
[[nodiscard]] CAS_API u64 GetFiberId() noexcept;

} // namespace CAS

#endif // CAS_INCLUDE_CAS_UTILS_SYSTEM_UTILS_H
