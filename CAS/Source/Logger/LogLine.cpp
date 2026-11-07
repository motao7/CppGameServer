#include <CAS/Logger/LogLine.h>

#include <array>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <memory>
#include <new>
#include <ostream>
#include <string_view>
#include <type_traits>
#include <vector>

#include <CAS/Logger/Buffer.h>
#include <CAS/Logger/BufferConfig.h>
#include <CAS/Macros.h>
#include <CAS/Utils/SystemUtils.h>
#include <CAS/Utils/ThreadUtils.h>

#include "Logger/LogRecord.h"
#include "Logger/LoggerImpl.h"

namespace CAS::Detail
{

// ============================================================
// LogLine::Impl —— 真正干活的部分，只在这里可见
//
// 为什么 Impl 能塞进 LogLine 内部那块定长存储（SBO）？
// **唯一的硬性前提是：sizeof(Impl) 是编译期常量，且 ≤ LOG_LINE_IMPL_SIZE。**
//
//   前提 1：Impl 必须是**完整类型**，sizeof / alignof 才算得出来。
//           定义放在这里（.cpp），头文件里只有 `struct Impl;` 前置声明——
//           这既是能算大小的前提，也顺带保住了实现隐藏。
//
//   前提 2：大小必须**可控**：成员全是定长大小的值类型（引用、整型、string_view、
//           time_point、InlineBuffer、streambuf、ostream），没有变长数组，
//           也没有"布局随实现/派生类变化"的子对象。
//           注意这**不等于"完全不碰堆"**：InlineBuffer 里就有 std::vector，
//           消息超长时它照样在堆上分配；但 vector 对象本身只占 3 个指针，
//           不改变 sizeof(Impl)。"对象本身放哪（栈内的定长存储）"和
//           "内容放哪（堆）"是两个正交问题，别混为一谈。
//
//   前提 3：能原地构造 / 析构。construct_at + destroy_at 就够，
//           调用点是静态类型，**不需要虚析构**（只有"通过基类指针 delete"才需要）。
//
// 补充：虚函数本身并不妨碍定长存储——含虚函数的类型大小同样编译期可知，
// 只是多 8 字节 vptr，且若通过基类指针管理就必须有虚析构。
// 真正会撑爆缓冲区的是"内嵌大对象/大数组"，而不是"有没有虚函数"。
//
// 一旦 Impl 真的超过缓冲区，下面的 static_assert 会编译失败，不会静默写穿内存。
// ============================================================
struct LogLine::Impl final
{
    // 注意这里是**带副作用**的构造函数：一条日志的"时间"和"发起者"在构造这一刻就定死。
    // 若等到析构提交时再取，`<<` 拼接期间的耗时会算进去，时间戳系统性滞后；
    // 跨线程时 m_threadName 更可能指向已经改名/退出的线程（见头文件类注释第 6 点）。
    Impl(Logger& logger, LogLevel::Level level, u32 line, std::string_view file_name)
        : m_logger(logger)
        , m_level(level)
        , m_line(line)
        , m_fileName(file_name)
        , m_timestamp(std::chrono::system_clock::now())
        , m_elapsed(GetElapsedTime())
        , m_threadId(GetThreadId())
        , m_fiberId(GetFiberId())
        , m_threadName(GetThreadName())
        , m_streamBuffer(m_inlineBuffer)   // streambuf 只绑引用，不拷贝数据
        , m_stream(&m_streamBuffer)        // ostream 的所有写入都落到 m_inlineBuffer
    {
    }

    // 把已经采好的字段原样装成 LogRecordView。
    // 这里用聚合初始化：字段顺序与 LogRecordView 的声明顺序一致，少写一堆赋值，
    // 也避免"漏填某个字段"（漏填会退化成默认值，编译器不报错，但日志里就是空的）。
    // 返回的视图是非拥有的，只在调用方（Logger::Impl::submit）同步使用期间有效。
    [[nodiscard]] LogRecordView getRecord() const noexcept
    {
        return {
            m_level,                 // LogLevel::Level
            m_logger.getName(),      // 日志器名（Logger 构造后不再改名，视图稳定）
            m_inlineBuffer.view(),   // 消息正文，指向本对象内部的缓冲
            m_timestamp,             // 墙上时间
            m_elapsed,               // 起服至今
            m_threadId,
            m_fiberId,
            m_threadName,            // TLS 视图：本对象存续期间该线程不改名才有效
            m_fileName,
            m_line,
        };
    }

    Logger&                                        m_logger;
    LogLevel::Level                                m_level;
    u32                                            m_line = 0;
    std::string_view                               m_fileName;

    // ---- 构造时采集的元信息（见构造函数注释）----
    std::chrono::system_clock::time_point          m_timestamp;
    std::chrono::steady_clock::duration            m_elapsed;
    u64                                            m_threadId = 0;
    u64                                            m_fiberId  = 0;
    std::string_view                               m_threadName;

    // ---- 消息缓冲 ----
    InlineBuffer<LOG_MESSAGE_INLINE_CAPACITY>      m_inlineBuffer;   // 短的走内联，长的自动溢出到堆
    SmallStreamBuffer<LOG_MESSAGE_INLINE_CAPACITY> m_streamBuffer;
    std::ostream                                   m_stream;
};

LogLine::LogLine(Logger& logger, LogLevel::Level level, u32 line,
                 std::string_view file_name) noexcept
{
    // 三道编译期安全网。缓冲区大小 LOG_LINE_IMPL_SIZE 是手写常量，
    // 与 Impl 的真实大小 / 对齐之间没有任何机制保证一致，只能靠 static_assert 兜住。
    // （断言写在成员函数里，是因为这几个实体都是 private。）
    static_assert(sizeof(Impl) <= LOG_LINE_IMPL_SIZE,
                  "LogLine inline implementation storage is too small");
    // Impl 若出现 alignas(32/64) 的成员，std::max_align_t 就不够用了
    static_assert(alignof(Impl) <= alignof(std::max_align_t),
                  "LogLine implementation requires excessive alignment");
    // ~LogLine() 声明为 noexcept，这条契约只能靠运行时 terminate 兑现；
    // 一旦 Impl 里有成员的析构可能抛异常，这里直接编译失败
    static_assert(std::is_nothrow_destructible_v<Impl>,
                  "~LogLine() is noexcept, so Impl must be nothrow destructible");

    // 就地构造：把 Impl 直接放在对象的字节存储里，不经过堆分配
    std::construct_at(reinterpret_cast<Impl*>(m_implStorage), logger, level, line, file_name);
}

LogLine::Impl& LogLine::getImpl() noexcept
{
    // std::launder：同一块存储上新构造对象后，必须通过它取得"指向新对象"的指针，
    // 否则编译器有权沿用构造前对该地址的假设（严格别名 / 常量折叠优化）。
    // 私有 + 只此一处：把 reinterpret_cast 这类危险操作收敛到唯一入口。
    return *std::launder(reinterpret_cast<Impl*>(m_implStorage));
}

LogLine::~LogLine() noexcept
{
    Impl& impl = getImpl();

    // 提交 → 销毁，顺序不能反：提交要用到 Impl 里的消息缓冲
    LoggerAccess::Submit(impl.m_logger, impl.getRecord());
    std::destroy_at(&impl);
}

std::ostream& LogLine::stream() noexcept
{
    return getImpl().m_stream;
}

// ============================================================
// LogPrintf —— printf 风格入口
// ============================================================
void LogPrintf(Logger& logger, LogLevel::Level level, u32 line,
               std::string_view file_name, const char* format, ...)
{
    // 先构造 LogLine：后面每个 return 分支都靠它的析构提交日志，
    // 不需要（也不能）手动调用提交，天然避免"某个分支忘了提交"。
    LogLine log_line(logger, level, line, file_name);

    // 空格式串不是编程错误，也不是"什么都不记"：留一个占位符，
    // 这样日志里能看出"这里调用过 LogPrintf，但参数是空的"
    if (format == nullptr)
    {
        log_line.stream() << "<null-format>";
        return;
    }

    // 栈上缓冲：绝大多数日志一行放得下，不值得为它分配堆内存
    std::array<char, PRINTF_FORMAT_INLINE_CAPACITY> inline_buffer;

    va_list arguments;
    va_list arguments_copy;   // 访问可变参数会破坏参数指针偏移量，为了能再次访问可变参数包，必须拷贝一份
    va_start(arguments, format);
    va_copy(arguments_copy, arguments);

    // 第一趟：直接往栈上缓冲写，顺便拿到"不含结尾 '\0' 的所需长度"
    const int required_size = std::vsnprintf(inline_buffer.data(), inline_buffer.size(), format, arguments);
    va_end(arguments);        // arguments 已被消费，用完立即结束，避免忘记

    if (required_size < 0)
    {
        // 编码错误 / 非法格式串等，vsnprintf 返回负值
        va_end(arguments_copy);
        log_line.stream() << "<format-error>";
        return;
    }

    const std::size_t message_size = static_cast<std::size_t>(required_size);

    // 严格小于：required_size 不包含结尾 '\0'，所以 < size 才说明完整放下了
    if (message_size < inline_buffer.size())
    {
        va_end(arguments_copy);
        log_line.stream().write(inline_buffer.data(), static_cast<std::streamsize>(message_size));
        return;   // 析构函数提交
    }

    // 装不下：按精确长度开一次堆缓冲，第二趟用 va_copy 的副本重新格式化，
    // 这样既不截断消息，也仍然只有一次堆分配。
    std::vector<char> overflow_buffer(message_size + 1);   // +1 给结尾 '\0'
    const int second_result = std::vsnprintf(overflow_buffer.data(), overflow_buffer.size(),
                                             format, arguments_copy);
    va_end(arguments_copy);   // 所有路径都要配对 va_end，否则是未定义行为

    if (second_result < 0)
    {
        log_line.stream() << "<format-error>";
        return;
    }

    log_line.stream().write(overflow_buffer.data(), static_cast<std::streamsize>(message_size));

    // 到这里还只是在内存里格式化这条日志；
    // 出了这个函数，log_line 析构，才把日志从内存提交到 logger 的 appender 中去
}

} // namespace CAS::Detail
