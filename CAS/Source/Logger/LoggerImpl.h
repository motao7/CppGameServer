#ifndef CAS_SRC_LOGGER_LOGGER_H
#define CAS_SRC_LOGGER_LOGGER_H

#include <atomic>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

#include <CAS/Export.h>
#include <CAS/Logger/Logger.hpp>
#include <CAS/Logger/LogLevel.h>

#include "Logger/Formatter.h"
#include "Logger/LogRecord.h"

namespace CAS
{

class Logger::Impl
{
public:
    explicit Impl(std::string name);

    [[nodiscard]] bool shouldLog(LogLevel::Level level) const noexcept;
    void submit(const Detail::LogRecordView& record) noexcept;

    void setLevel(LogLevel::Level level) noexcept;
    [[nodiscard]] LogLevel::Level getLevel() const noexcept;
    [[nodiscard]] std::string_view getName() const noexcept;

    void setFormatter(std::shared_ptr<const Detail::Formatter> formatter);
    [[nodiscard]] std::shared_ptr<const Detail::Formatter> getFormatter() const noexcept;

    void addAppender(AppenderPtr appender);
    void removeAppender(const AppenderPtr& appender);
    void clearAppenders();

    void flush();
    void sync();

    void setRoot(LoggerPtr root) noexcept;

private:
    std::string m_name;
    std::atomic<LogLevel::Level> m_level{LogLevel::Level::LOG_LV_DEBUG};

    mutable std::shared_mutex m_mutex;
    std::shared_ptr<const Detail::Formatter> m_formatter;
    std::vector<AppenderPtr> m_appenders;
    LoggerPtr m_root;
};

namespace Detail
{

class LoggerAccess final
{
public:
    static void Submit(Logger& logger, const LogRecordView& record) noexcept;
    static void SetRoot(Logger& logger, LoggerPtr root) noexcept;
};

} // namespace Detail

} // namespace CAS

#endif // CAS_SRC_LOGGER_LOGGER_H
