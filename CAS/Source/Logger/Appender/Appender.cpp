#include <CAS/Logger/Appender/Appender.h>

#include <cstdio>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include "CAS/Macros.h"

namespace CAS
{

// ============================================================
// Appender::Impl —— 基类实现：等级过滤 + 加锁 + 转发到钩子
// ============================================================
void Appender::Impl::append(LogLevel::Level level,
                            std::string_view formatted_record) noexcept
{
    if(static_cast<u8>(level) < static_cast<u8>(getLevel()))
    {
        return;
    }

    try
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        writeUnlocked(formatted_record);
        if(level == LogLevel::Level::LOG_LV_FATAL)
        {
            flushUnlocked();
        }
    }
    catch(...)
    {
        constexpr std::string_view message = "CAS Logger : Appender Operation Failde.\n";
        std::fwrite(message.data(), 1 , message.size(), stderr);
    }
    
}

void Appender::Impl::setLevel(LogLevel::Level level) noexcept
{
    m_level.store(level, std::memory_order_release);
}

LogLevel::Level Appender::Impl::getLevel() const noexcept
{
    return m_level.load(std::memory_order_acquire);
}

void Appender::Impl::flush()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    flushUnlocked();
}

void Appender::Impl::sync()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    syncUnlocked();
}

// ============================================================
// Appender —— 门面：只做转发，不做任何逻辑
// ============================================================
Appender::Appender(std::unique_ptr<Impl> impl) noexcept
    : m_impl(std::move(impl))
{
}

Appender::~Appender() = default;

void Appender::setLevel(LogLevel::Level level) noexcept
{
    m_impl->setLevel(level);
}

LogLevel::Level Appender::getLevel() const noexcept
{
    return m_impl->getLevel();
}

void Appender::flush()
{
    m_impl->flush();
}

void Appender::sync()
{
    m_impl->sync();
}

namespace
{

// ---------- 标准输出 ----------
class StdoutAppenderImpl final : public Appender::Impl
{
protected:
    void writeUnlocked(std::string_view formatted_record) noexcept override
    {
        std::fwrite(formatted_record.data(), 1, formatted_record.size(), stdout);
    }

    void flushUnlocked() noexcept override
    {
        std::fflush(stdout);
    }

    void syncUnlocked() noexcept override
    {
        flushUnlocked();
    }
};

// ---------- 文件输出 ----------
class FileAppenderImpl final : public Appender::Impl
{
public:
    explicit FileAppenderImpl(std::string file_name)
    {
        if(file_name.empty())
        {
            throw std::invalid_argument("Logger File Name Cannot be Empty");
        }

        m_file = std::fopen(file_name.c_str(), "ab");
        if(m_file == nullptr)
        {
            throw std::system_error(errno, std::generic_category(), "Failed to open Logger File :" +file_name);
        }
    }

    ~FileAppenderImpl()
    {
        if(m_file != nullptr)
        {
            std::fflush(m_file);
            std::fclose(m_file);
        }
    }

protected:
    void writeUnlocked(std::string_view formatted_record) noexcept override
    {
        if (!m_file)
        {
            return;
        }

        std::fwrite(formatted_record.data(), 1, formatted_record.size(), m_file);
    }

    void flushUnlocked() noexcept override
    {
        if (m_file)
        {
            std::fflush(m_file);
        }
    }

    void syncUnlocked() noexcept override
    {
#if defined(_WIN32)
        ::_commit(::_fileno(m_file));
#else
        ::fdatasync(::fileno(m_file));
#endif
    }

private:
    std::FILE* m_file = nullptr;
};

} // namespace

// ============================================================
// Detail::AppenderAccess —— 唯一构造入口
// ============================================================
namespace Detail
{

AppenderPtr AppenderAccess::MakeStdoutAppender()
{
    return AppenderPtr(new Appender(std::make_unique<StdoutAppenderImpl>()));
}

AppenderPtr AppenderAccess::MakeFileAppender(std::string file_name)
{
    return AppenderPtr(new Appender(
        std::make_unique<FileAppenderImpl>(std::move(file_name))));
}

void AppenderAccess::Append(const AppenderPtr& appender,
                            LogLevel::Level level,
                            std::string_view formatted_record) noexcept
{
    if (!appender)
    {
        return;
    }
    appender->m_impl->append(level, formatted_record);
}

} // namespace Detail

// ============================================================
// 对外工厂
// ============================================================
AppenderPtr MakeStdoutAppender()
{
    return Detail::AppenderAccess::MakeStdoutAppender();
}

AppenderPtr MakeFileAppender(std::string file_name)
{
    return Detail::AppenderAccess::MakeFileAppender(std::move(file_name));
}

} // namespace CAS
