#ifndef CAS_INCLUDE_CAS_LOGGER_LOGGER_HPP
#define CAS_INCLUDE_CAS_LOGGER_LOGGER_HPP

#include <memory>
#include <string>
#include <string_view>

#include <CAS/Export.h>
#include <CAS/Logger/Appender/Appender.hpp>
#include <CAS/Logger/LogLevel.h>

namespace CAS
{

namespace Detail
{
class LoggerAccess;
}

// 日志器门面（前端）。
// - Pimpl：Impl 里放 appender 列表、formatter、名字、等级等实现细节。
// - 与 Appender 不同：Logger 允许直接构造（带默认名 "root"），
//   但通常应通过 GetRootLogger / GetLogger 从全局注册表取。
class CAS_API Logger final
{
public:
    class Impl;

    explicit Logger(std::string name = "root");
    ~Logger();

    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&)                 = delete;
    Logger& operator=(Logger&&)      = delete;

    // 等级过滤：内部通常是 level >= 当前 Logger 等级 且至少有一个 Appender 接受
    [[nodiscard]] bool shouldLog(LogLevel::Level level) const noexcept;

    void setLevel(LogLevel::Level level) noexcept;
    [[nodiscard]] LogLevel::Level getLevel() const noexcept;

    [[nodiscard]] std::string_view getName() const noexcept;

    void setFormatter(std::string_view pattern);
    [[nodiscard]] std::string getFormatterPattern() const;

    void addAppender(AppenderPtr appender);
    void removeAppender(const AppenderPtr& appender);
    void clearAppenders();

    void flush();
    void sync();

private:
    std::unique_ptr<Impl> m_impl;

    friend class Detail::LoggerAccess;
};

using LoggerPtr = std::shared_ptr<Logger>;

[[nodiscard]] CAS_API LoggerPtr GetRootLogger();
[[nodiscard]] CAS_API LoggerPtr GetLogger(std::string_view name);

} // namespace CAS

#endif // CAS_INCLUDE_CAS_LOGGER_LOGGER_HPP
