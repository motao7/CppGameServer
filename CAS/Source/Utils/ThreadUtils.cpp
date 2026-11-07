#include <CAS/Utils/ThreadUtils.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#if defined(__linux__)
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>
#elif defined(_WIN32)
// 排除 shell / 图形界面等冷门 API，缩短编译时间、减少符号冲突
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// 阻止 windows.h 在全局命名空间定义 min / max 宏
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <CAS/Macros.h>
#include <CAS/Utils/StringUtils.h>

namespace CAS
{

namespace
{

#if defined(_WIN32)
struct LocalFreeDeleter
{
    void operator()(wchar_t* value) const noexcept
    {
        if (value != nullptr)
        {
            (void)::LocalFree(value);
        }
    }
};
#endif

[[nodiscard]] std::string GetNativeThreadName() noexcept
{
    try
    {
#if defined(__linux__)
        std::array<char, 16> native_name{};
        if (pthread_getname_np(pthread_self(), native_name.data(), native_name.size()) != 0)
        {
            return {};
        }
        return std::string(native_name.data());
#elif defined(_WIN32)
        PWSTR raw_name = nullptr;
        if (FAILED(::GetThreadDescription(::GetCurrentThread(), &raw_name)))
        {
            return {};
        }

        // GetThreadDescription 用 LocalAlloc 分配，必须 LocalFree 释放
        const std::unique_ptr<wchar_t, LocalFreeDeleter> native_name(raw_name);
        if (native_name == nullptr)
        {
            return {};
        }
        return StringUtils::WStringToString(std::wstring_view(native_name.get()));
#else
        return {};
#endif
    }
    catch (...)
    {
        return {};
    }
}

void SetNativeThreadName(std::string_view name) noexcept
{
#if defined(__linux__)
    // Linux 线程名最多 16 字节（含结尾 '\0'），即最多 15 个有效字节
    std::array<char, 16> native_name{};
    std::size_t name_size = std::min(name.size(), native_name.size() - 1);

    // 截断时回退到 UTF-8 字符边界，避免把一个多字节字符切成两半
    if (name_size < name.size())
    {
        while (name_size > 0 &&
               (static_cast<unsigned char>(name[name_size]) & 0xC0U) == 0x80U)
        {
            --name_size;
        }
    }

    std::memcpy(native_name.data(), name.data(), name_size);
    (void)pthread_setname_np(pthread_self(), native_name.data());
#elif defined(_WIN32)
    try
    {
        const std::wstring wide_name = StringUtils::StringToWString(name);
        if (!wide_name.empty())
        {
            (void)::SetThreadDescription(::GetCurrentThread(), wide_name.c_str());
        }
    }
    catch (...)
    {
    }
#else
    (void)name;
#endif
}

constexpr std::string_view DEFAULT_THREAD_NAME = "UNKNOWN";

// 每个线程各有一份独立的线程名，首次访问时初始化为系统原生线程名
thread_local std::string t_thread_name = GetNativeThreadName();

} // namespace

u64 GetThreadId() noexcept
{
#if defined(__linux__)
    // 内核 TID，可与 top -H / ps -T / perf / gdb 对齐
    static thread_local const u64 THREAD_ID = static_cast<u64>(::syscall(SYS_gettid));
#elif defined(_WIN32)
    static thread_local const u64 THREAD_ID = static_cast<u64>(::GetCurrentThreadId());
#else
    static thread_local const u64 THREAD_ID =
        static_cast<u64>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
#endif
    return THREAD_ID;
}

void SetThreadName(std::string name)
{
    if (name.empty())
    {
        // 空字符串 = 重置为系统原生线程名
        ASSERT_NOEFFECT2(false, "you passed empty thread name");

        std::string native_name = GetNativeThreadName();
        t_thread_name = native_name.empty() ? std::string(DEFAULT_THREAD_NAME)
                                            : std::move(native_name);
        return;
    }

    t_thread_name = std::move(name);
    SetNativeThreadName(t_thread_name);
}

std::string_view GetThreadName() noexcept
{
    return t_thread_name;
}

} // namespace CAS
