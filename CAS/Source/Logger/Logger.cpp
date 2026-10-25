#include "Logger/LoggerImpl.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <CAS/Macros.h>
#include <CAS/Logger/Appender/Appender.h>

namespace CAS
{

namespace
{

// 默认格式：时间.微秒 [等级] [日志器名] 消息 + 换行
constexpr std::string_view DEFAULT_LOG_PATTERN = "%d{%Y-%m-%d %H:%M:%S}.%u [%p] [%c] %m%n";

} // namespace

// ============================================================
// Logger::Impl —— 真正的实现
// ============================================================
Logger::Impl::Impl(std::string name)
    : m_name(std::move(name))
{
    // 默认格式器；Formatter 构造失败会抛 std::invalid_argument
    m_formatter = std::make_shared<Detail::Formatter>(DEFAULT_LOG_PATTERN);
}

bool Logger::Impl::shouldLog(LogLevel::Level level) const noexcept
{
    if (static_cast<u8>(level) < static_cast<u8>(getLevel()))
    {
        return false;
    }

    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        if (!m_appenders.empty())
        {
            return true;
        }
    }

    return m_root != nullptr && m_root->shouldLog(level);
}

void Logger::Impl::submit(const Detail::LogRecordView& record) noexcept
{
    if (static_cast<u8>(record.m_level) < static_cast<u8>(getLevel()))
    {
        return;
    }

    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if (m_appenders.empty())
    {
        lock.unlock();
        if (m_root != nullptr)
        {
            m_root->m_impl->submit(record);
        }
        return;
    }

    try
    {
        Detail::FormattedRecordBuffer formatted_record;
        m_formatter->format(record, formatted_record);
        for (const AppenderPtr& appender : m_appenders)
        {
            Detail::AppenderAccess::Append(appender, record.m_level, formatted_record.view());
        }
    }
    catch (...)
    {
        // 日志系统绝不允许把异常抛回业务侧
    }
}

void Logger::Impl::setLevel(LogLevel::Level level) noexcept
{
    m_level.store(level, std::memory_order_release);
}

LogLevel::Level Logger::Impl::getLevel() const noexcept
{
    return m_level.load(std::memory_order_acquire);
}

std::string_view Logger::Impl::getName() const noexcept
{
    // m_name 构造后不再修改，无需加锁
    return m_name;
}

void Logger::Impl::setFormatter(std::shared_ptr<const Detail::Formatter> formatter)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_formatter = std::move(formatter);
}

std::shared_ptr<const Detail::Formatter> Logger::Impl::getFormatter() const noexcept
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_formatter;
}

void Logger::Impl::addAppender(AppenderPtr appender)
{
    if (!appender)
    {
        return;
    }

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (std::find(m_appenders.begin(), m_appenders.end(), appender) == m_appenders.end())
    {
        m_appenders.push_back(std::move(appender));
    }
}

void Logger::Impl::removeAppender(const AppenderPtr& appender)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_appenders.erase(
        std::remove(m_appenders.begin(), m_appenders.end(), appender),
        m_appenders.end());
}

void Logger::Impl::clearAppenders()
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_appenders.clear();
}

void Logger::Impl::flush()
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if (m_appenders.empty())
    {
        lock.unlock();
        // m_root 是"构造期写一次、发布后只读"的成员，不存在并发写，自然不需要读锁。
        if (m_root != nullptr)
        {
            m_root->flush();
        }
        return;
    }

    for (const auto& appender : m_appenders)
    {
        appender->flush();
    }
}

void Logger::Impl::sync()
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if (m_appenders.empty())
    {
        lock.unlock();
        if (m_root != nullptr)
        {
            m_root->sync();
        }
        return;
    }

    for (const auto& appender : m_appenders)
    {
        appender->sync();
    }
}

void Logger::Impl::setRoot(LoggerPtr root) noexcept
{
    m_root = std::move(root);
}

// ============================================================
// Logger —— 门面：只做转发
// ============================================================
Logger::Logger(std::string name)
    : m_impl(std::make_unique<Impl>(std::move(name)))
{
}

Logger::~Logger() = default;

bool Logger::shouldLog(LogLevel::Level level) const noexcept
{
    return m_impl->shouldLog(level);
}

void Logger::setLevel(LogLevel::Level level) noexcept
{
    m_impl->setLevel(level);
}

LogLevel::Level Logger::getLevel() const noexcept
{
    return m_impl->getLevel();
}

std::string_view Logger::getName() const noexcept
{
    return m_impl->getName();
}

void Logger::setFormatter(std::string_view pattern)
{
    m_impl->setFormatter(std::make_shared<Detail::Formatter>(pattern));
}

std::string Logger::getFormatterPattern() const
{
    const auto formatter = m_impl->getFormatter();
    return formatter ? formatter->getPattern() : std::string{};
}

void Logger::addAppender(AppenderPtr appender)
{
    m_impl->addAppender(std::move(appender));
}

void Logger::removeAppender(const AppenderPtr& appender)
{
    m_impl->removeAppender(appender);
}

void Logger::clearAppenders()
{
    m_impl->clearAppenders();
}

void Logger::flush()
{
    m_impl->flush();
}

void Logger::sync()
{
    m_impl->sync();
}

// ============================================================
// Detail::LoggerAccess —— 唯一的内部访问入口
// ============================================================
namespace Detail
{

void LoggerAccess::Submit(Logger& logger, const LogRecordView& record) noexcept
{
    logger.m_impl->submit(record);
}

void LoggerAccess::SetRoot(Logger& logger, LoggerPtr root) noexcept
{
    logger.m_impl->setRoot(std::move(root));
}

} // namespace Detail

// ============================================================
// LoggerManager —— 日志器注册表（单例，内部使用）
// ============================================================
class LoggerManager final
{
public:
    LoggerManager()
        : m_root(std::make_shared<Logger>("root"))
    {
        m_root->addAppender(MakeStdoutAppender());
        m_loggers.emplace("root", m_root);
    }

    [[nodiscard]] LoggerPtr getRoot() const noexcept
    {
        // m_root 构造期写入，之后不再修改 → 无需加锁
        return m_root;
    }

    [[nodiscard]] LoggerPtr getLogger(std::string_view name);

private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, LoggerPtr> m_loggers;
    LoggerPtr m_root;
};

LoggerPtr LoggerManager::getLogger(std::string_view name)
{
    ASSERT_RETVAL2(!name.empty(), nullptr, "logger name cannot be empty");

    std::lock_guard<std::mutex> lock(m_mutex);

    const auto it = m_loggers.find(std::string(name));
    if (it != m_loggers.end())
    {
        return it->second;
    }

    // 先挂 root，再发布到注册表：
    // 保证其他线程拿到这个 Logger 时，m_root 已经是最终值（之后不再修改）
    auto logger = std::make_shared<Logger>(std::string(name));
    Detail::LoggerAccess::SetRoot(*logger, m_root);

    m_loggers.emplace(logger->getName(), logger);
    return logger;
}

LoggerManager& GetLoggerManager()
{
    static LoggerManager manager;
    return manager;
}

// ============================================================
// 对外入口
// ============================================================
LoggerPtr GetRootLogger()
{
    return GetLoggerManager().getRoot();
}

LoggerPtr GetLogger(std::string_view name)
{
    return GetLoggerManager().getLogger(name);
}

} // namespace CAS
