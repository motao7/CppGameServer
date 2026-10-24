#ifndef CAS_INCLUDE_CAS_LOGGER_APPENDER_APPENDER_HPP
#define CAS_INCLUDE_CAS_LOGGER_APPENDER_APPENDER_HPP

#include <memory>
#include <string>
#include <string_view>

#include <CAS/Export.h>
#include <CAS/Logger/LogLevel.h>

namespace CAS
{

namespace Detail
{
class AppenderAccess;
}

// 日志输出端（sink）的门面类。
// - Pimpl：真正的实现藏在 Impl 中，头文件不暴露 <mutex>/<atomic>/文件 IO 等细节。
// - 构造私有：外部只能通过 MakeStdoutAppender / MakeFileAppender 创建实例。
// - 禁止拷贝与移动：Appender 代表一个全局唯一的输出端身份，用 AppenderPtr 共享。
class CAS_API Appender final
{
public:
    class Impl;

    // 析构必须定义在 Impl 已完整的翻译单元中（Pimpl 的标准写法）
    ~Appender();

    Appender(const Appender&)            = delete;
    Appender& operator=(const Appender&) = delete;
    Appender(Appender&&)                 = delete;
    Appender& operator=(Appender&&)      = delete;

    void setLevel(LogLevel::Level level) noexcept;
    [[nodiscard]] LogLevel::Level getLevel() const noexcept;

    void flush();
    void sync();

private:
    explicit Appender(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> m_impl;

    friend class Detail::AppenderAccess;
};

// 输出端一律用 shared_ptr 共享：多个 logger 可以挂同一个 sink。
using AppenderPtr = std::shared_ptr<Appender>;

[[nodiscard]] CAS_API AppenderPtr MakeStdoutAppender();
[[nodiscard]] CAS_API AppenderPtr MakeFileAppender(std::string file_name);

} // namespace CAS

#endif // CAS_INCLUDE_CAS_LOGGER_APPENDER_APPENDER_HPP
