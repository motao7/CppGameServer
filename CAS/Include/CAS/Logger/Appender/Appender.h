#ifndef CAS_INCLUDE_CAS_LOGGER_APPENDER_APPENDER_H
#define CAS_INCLUDE_CAS_LOGGER_APPENDER_APPENDER_H

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

#include <CAS/Logger/Appender/Appender.hpp>

namespace CAS
{

// Appender 的唯一实现基类。
// 派生类只实现三个 *Unlocked 钩子；加锁、等级过滤等“不变量”由基类的
// 公开非虚函数统一保证，派生类无法绕过（NVI：Non-Virtual Interface）。
class Appender::Impl
{
public:
    virtual ~Impl() = default;

    virtual void append(LogLevel::Level level,
                        std::string_view formatted_record) noexcept;

    void setLevel(LogLevel::Level level) noexcept;
    [[nodiscard]] LogLevel::Level getLevel() const noexcept;

    void flush();
    void sync();

protected:
    // 以下三个钩子调用时必须已经持有 m_mutex（因此以 Unlocked 结尾）
    virtual void writeUnlocked(std::string_view formatted_record) noexcept = 0;
    virtual void flushUnlocked() noexcept = 0;
    virtual void syncUnlocked() noexcept = 0;

    std::mutex m_mutex;

private:
    std::atomic<LogLevel::Level> m_level{LogLevel::Level::LOG_LV_DEBUG};
};

namespace Detail
{

// 唯一被授权构造 Appender 的入口（passkey / friend 模式）。
// 用户代码只能通过 AppenderAccess 提供的工厂函数拿到 AppenderPtr。
class AppenderAccess final
{
public:
    [[nodiscard]] static AppenderPtr MakeStdoutAppender();
    [[nodiscard]] static AppenderPtr MakeFileAppender(std::string file_name);

    static void Append(const AppenderPtr& appender,
                       LogLevel::Level level,
                       std::string_view formatted_record) noexcept;
};

} // namespace Detail

} // namespace CAS

#endif // CAS_INCLUDE_CAS_LOGGER_APPENDER_APPENDER_H
