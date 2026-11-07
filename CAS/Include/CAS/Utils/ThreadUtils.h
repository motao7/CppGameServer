#ifndef CAS_INCLUDE_CAS_UTILS_THREAD_UTILS_H
#define CAS_INCLUDE_CAS_UTILS_THREAD_UTILS_H

#include <string>
#include <string_view>

#include <CAS/Export.h>
#include <CAS/Macros.h>

namespace CAS
{

/**
 * @brief 获取当前线程 ID
 *
 * Linux 下返回内核 TID（可与 top -H / ps -T / perf / gdb 对齐）；
 * Windows 下返回系统线程 ID；
 * 其他平台返回 std::thread::id 的哈希值。
 */
[[nodiscard]] CAS_API u64 GetThreadId() noexcept;

/**
 * @brief 设置当前线程名称
 *
 * 传入空字符串表示"重置为系统原生线程名"。
 * 注意 Linux 的线程名最多 15 字节，超长会被截断（按 UTF-8 字符边界）。
 */
CAS_API void SetThreadName(std::string name);

/**
 * @brief 获取当前线程名称（返回的是 thread_local 缓冲的视图，不可长期持有）
 */
[[nodiscard]] CAS_API std::string_view GetThreadName() noexcept;

} // namespace CAS

#endif // CAS_INCLUDE_CAS_UTILS_THREAD_UTILS_H
