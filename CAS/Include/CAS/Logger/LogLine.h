#ifndef CAS_INCLUDE_CAS_LOGGER_LOG_LINE_H
#define CAS_INCLUDE_CAS_LOGGER_LOG_LINE_H

#include <cstddef>
#include <iosfwd>
#include <string_view>

#include <CAS/Export.h>
#include <CAS/Logger/LogLevel.h>
#include <CAS/Macros.h>

namespace CAS
{

// 前置声明：这里只需要 Logger 的引用，不必把整个 Logger.hpp 拖进来（规范 4.4）
class Logger;

namespace Detail
{

/**
 * @brief 一条日志的收集器：**构造即开始收集，析构即提交**（RAII）。
 *
 * 业务代码不直接使用，由日志宏封装：
 *
 * @code
 * CAS_LOG_INFO(logger) << "x = " << x;
 * @endcode
 *
 * 宏展开后必须是一条**表达式**（因为后面 `<< "x = " << x` 是用户自己写的尾巴，
 * 要能续在宏展开结果之后），所以宏只负责产出"表达式的头部"：
 *
 * @code
 * // 注意 .stream()：LogLine 本身没有 operator<<，只有 stream()
 * for (bool once = logger.shouldLog(level); once; once = false)
 *     CAS::Detail::LogLine(logger, level, __LINE__, __FILE__).stream() << "x = " << x;
 * @endcode
 *
 * 这里的 `LogLine(...)` 是**临时对象**，生命周期到整条语句的分号为止；
 * 析构时消息已经拼完，正好由析构函数提交。
 *
 * ---
 *
 * ### 1. 为什么用「构造 / 析构」，而不是一个 `Log(logger, level, message)` 或显式 `submit()`
 *
 * 有两个互相独立的原因，缺一不可。
 *
 * **① 语法层面：显式 `submit()` 根本写不出来。**
 * 宏只占据一行的开头，`<< a << b` 这段尾巴是用户写的，宏无法在它之后追加任何调用：
 *  - 写成 `do { } while (0)` 块式宏 → 展开结果是"语句"，用户在后面接 `<<` 直接编译错误；
 *  - 写成"先声明对象、再让用户拼接"→ 展开成两条语句，用户根本不知道变量名，
 *    `submit()` 永远不会被调用；放进 `if (c) LOG << x;` 里还会只让第一条语句受管辖。
 * 唯一可行的形态是「表达式」：让宏产出一个临时对象，提交动作挂在它的析构上。
 *
 * **② 语义层面：即使写出来了，也会漏日志。**
 * 具名对象 + 手动 `submit()` 的写法，任何 `return` / `throw` / `break` / `continue` /
 * `goto` 都会跳过那行调用，日志静默消失。析构函数由编译器在**每条退出路径**上插入，
 * 绕不过去；异常栈展开时同样会执行。所以析构函数必须 `noexcept` ——
 * 它可能在栈展开过程中执行，此时再抛异常会直接 terminate（详见 `.cpp` 实现）。
 *
 * **③ 时机也刚好。** 临时对象在"完整表达式"结束（分号）时析构，
 * 此刻 `<< a << b` 全部写完，缓冲区里是完整消息。
 *
 * ### 2. 为什么要 Pimpl，而不是把成员直接写在头文件里
 *
 * 成员里要放 `std::ostream`、`std::streambuf`、消息缓冲等一整套东西。
 * 直接暴露会让每个调用方都 include `<ostream>` / `<vector>` / formatter，
 * 编译时间成倍增长；更关键的是**公开头文件一旦改内部布局就要全量重编**。
 * Pimpl 让头文件只承诺"有一个不透明实现"，实现随便改，ABI 不变。
 *
 * ### 3. 为什么 Impl 不放在堆上（这是最关键的一条）
 *
 * 常规 Pimpl 是 `std::unique_ptr<Impl>`，每条日志一次 `new` + 一次 `delete`。但：
 *  - 日志是**热路径**：一帧几百条日志时，堆分配会成为帧率杀手；
 *  - `operator new` 内部有 arena 锁，多线程打日志会在这把锁上串行化；
 *  - 分配 / 释放交替进行，会造成内存碎片和 cache miss。
 *
 * 所以这里改成 **SBO（Small Buffer Optimization）/ 就地 Pimpl**：
 * 在对象内部预留一块 `LOG_LINE_IMPL_SIZE` 字节的原始存储，`Impl` 直接
 * placement new 在这块存储上，`LogLine` 是栈上临时对象 —— **一次日志零堆分配**。
 *
 * 代价：`sizeof(LogLine)` 变成 1KB 左右（栈占用），且 `Impl` 超过缓冲区会在
 * 编译期报错（`.cpp` 里的 `static_assert` 就是这道安全网）。
 * 这是一次明确的**空间换时间**：栈空间便宜（几 MB），堆分配贵。
 *
 * ### 4. 为什么删除拷贝和移动
 *
 * 对象内含一块**未类型化的字节存储**，编译器生成的拷贝构造只会按位复制：
 * 复制出来的 `LogLine` 里，`Impl` 的 `std::ostream` 仍然指回**原来那个对象**的
 * streambuf 和消息缓冲。于是两个对象析构时会对同一份数据提交两次
 * （日志重复），或访问已销毁的流（悬垂引用）。
 *
 * 正确实现深拷贝需要"复制构造一个 Impl"，而 `std::ostream` 本身不可拷贝，
 * 这个语义根本不存在。同时它只是宏里的临时量，**没有复制或移动的需求**，
 * 所以直接 `= delete` 把这四个入口全部堵死，让误用在编译期暴露。
 *
 * ### 5. 为什么 `stream()` 返回 `std::ostream&`，而不是 `LogLine& operator<<`
 *
 * 因为日志要支持**任意类型**（int、double、C 字符串、std::string、玩家自定义结构体……），
 * 成员 `operator<<` 只有写成**函数模板**才能做到；而非模板重载无法穷举，
 * 也永远支持不了调用方的自定义类型。
 *
 * 模板一旦出现，函数体就必须放在头文件里（模板在**每个调用点**现场实例化，
 * 只有声明、定义在 .cpp 里会直接链接错误；显式实例化又要求预先枚举所有类型，
 * 而类型集合是开放的）。后果是连锁的：
 *  - `m_stream << value` 需要 Impl 的完整定义 → 头文件里不能再是 `struct Impl;`，
 *    Pimpl 就地失效，内部布局对所有人可见；
 *  - `<ostream>` / `<streambuf>` / `<vector>` 被拖进每个打日志的 .cpp，
 *    改一次 Impl 就要全量重编；
 *  - 每种类型 × 每个调用点各生成一份实例化代码，二进制膨胀。
 *
 * 返回标准 `std::ostream&` 后，`<<` 全部走标准库**已有的**重载（大多是普通成员函数，
 * 编译期绑定，没有新实例化），Impl 继续藏在 .cpp 里。
 *
 * 注意：`std::ostream` 的写入最终仍要经过 `std::streambuf` 的虚函数
 * （`xsputn` / `overflow`，见 `Buffer.h`），**这次虚调用两个方案都有**，
 * 不是返回 `ostream&` 省下的东西。
 *
 * ### 6. 为什么元信息在「构造时」就采集完，而不是等析构提交时再取
 *
 * `Impl` 的构造函数里一次性把 `m_timestamp` / `m_elapsed` / `m_threadId` /
 * `m_fiberId` / `m_threadName` 全部取好，析构时只是把这些字段原样装进
 * `LogRecordView` 交给 Logger。
 *
 * 理由是**语义**：日志记录的是"事件发生的那一刻"，不是"消息最终写进文件的那一刻"。
 *  - `<<` 拼接本身可能耗时（格式化一个大对象、查一次哈希表），
 *    若等到析构再取时间，时间戳会系统性滞后，排查时序问题时误差可达毫秒级；
 *  - 跨线程时更明显：`m_threadName` 是 TLS 视图，等到提交时那条线程可能已经
 *    改名甚至退出，采到的是"错的线程"。
 *
 * 代价：即使这条日志最终被等级过滤掉，时间 / 线程 ID 也已经取了。
 * 但这些取用都很便宜（`GetThreadId` / `GetThreadName` 是 thread_local 缓存，
 * `system_clock::now()` 走 vDSO），相比一次堆分配可以忽略；
 * 真正想省成本应该把 `shouldLog` 的判断提到宏入口，连 `LogLine` 都不构造。
 *
 * 顺带说明：本类**不做等级过滤**，过滤在 `Logger::Impl::submit` 内部完成——
 * 单一职责：`LogLine` 只管"把一条日志收集完整"。
 */
class CAS_API LogLine final
{
public:
    /**
     * @brief 开始收集一条日志
     *
     * 构造时就把时间戳 / 耗时起点 / 线程 ID / 纤程 ID / 线程名一次采齐（见类注释第 6 点），
     * 因此这个构造函数是"带副作用"的：它代表**事件发生的时刻**。
     *
     * `noexcept`：整个构造过程零堆分配（`InlineBuffer` 内部的 `std::vector` 只是默认构造，
     * `std::ostream` 只是绑定 streambuf），不会失败；一旦允许它抛异常，
     * 就会从日志宏一路穿到业务代码里。
     *
     * @param logger 目标日志器（引用，不拥有；必须比本对象活得久）
     * @param level  日志等级，最终由 logger 再做一次等级过滤
     * @param line   行号，通常由宏传 `__LINE__`
     * @param file_name 文件名，通常由宏传 `__FILE__`
     */
    LogLine(Logger& logger, LogLevel::Level level, u32 line,
            std::string_view file_name) noexcept;

    /// 析构时提交日志；绝不抛异常（可能处于栈展开过程）
    ~LogLine() noexcept;

    // 见类注释第 4 点：按位复制会破坏 Impl 的内部指针，语义上也没有复制需求
    LogLine(const LogLine&)            = delete;
    LogLine& operator=(const LogLine&) = delete;
    LogLine(LogLine&&)                 = delete;
    LogLine& operator=(LogLine&&)      = delete;

    /**
     * @brief 取得用于拼接消息的流
     * @return 引用，析构时自动把累积的内容提交给 Logger
     */
    [[nodiscard]] std::ostream& stream() noexcept;

private:
    // 只在 .cpp 中定义的实现，大小 / 对齐只在编译单元内可见
    struct Impl;

    // 缓冲区容量：兼顾"绝大多数日志放得下"和"栈占用不夸张"
    static constexpr std::size_t LOG_LINE_IMPL_SIZE = 1024;

    // ------------------------------------------------------------------
    // 这一行是整个设计的地基，拆开看三个部分：
    //
    // 【1】std::byte m_implStorage[N] —— 为什么用 std::byte 而不是 char
    //   这块内存还没有对象，只是"原始存储"。std::byte 是 C++17 专门为此引入的
    //   类型：它不参与算术、不隐式转换成数值，语义上就是"字节"，用它最不容易
    //   被误当成字符数组去读写（char 会让人以为这里是字符串）。
    //   （std::aligned_storage_t 在 C++23 已废弃，std::byte[] + alignas 是现行写法。）
    //
    // 【2】alignas(std::max_align_t) —— 为什么必须显式指定对齐
    //
    //   先明确 alignof 是什么：**alignof(T) 表示 T 的对象的起始地址必须是它的整数倍**。
    //   alignof(char) == alignof(std::byte) == 1（= 地址不受任何限制），
    //   alignof(int) == 4，alignof(double) == alignof(void*) == 8，
    //   alignof(std::max_align_t) 在 x86-64 上通常是 16。
    //   注意它说的是"地址能被几整除"，不是"占几个字节"。
    //
    //   数组的对齐继承自元素类型，所以 std::byte[1024] 的 alignof 也是 1。
    //   而**成员偏移由成员自己的对齐要求决定**，alignof == 1 意味着不需要填充：
    //
    //       struct Wrapper {
    //           char      m_tag;               // 偏移 0
    //           std::byte m_storage[16];       // 偏移 1 ！因为不需要对齐
    //       };
    //
    //       偏移: 0      1                                  17
    //             [tag ][m_storage .......................]
    //       地址:  0x1000 0x1001  ← 奇数地址，不是 16 的倍数
    //
    //   而 Impl 里有 std::ostream、std::streambuf 子对象、std::vector 的三个指针，
    //   这些成员要求 8 或 16 字节对齐。把 Impl placement new 到 0x1001 这种地址上，
    //   等于让"要求 16 对齐"的对象住在"只保证 1 对齐"的地方 —— **未定义行为**：
    //    - 标准层面：UB 意味着编译器不需要产生任何正确结果，"今天能跑"不作数；
    //    - 硬件层面：x86-64 的普通 mov 容忍非对齐（只是拆成两次访问、变慢），
    //      所以问题会潜伏；但 SSE 的 movaps/movdqa 要求 16 对齐，会直接 #GP；
    //      ARM64 的原子指令（ldaxr/stxr）、NEON 同样要求对齐，否则异常；
    //      ARMv7 / 部分 RISC-V 配置连普通访问都会 SIGBUS。
    //      而 Impl 里的 std::shared_ptr 引用计数就是 std::atomic —— 真的会炸。
    //    - 编译器层面：最阴的一条。alignof(Impl) == 16 是它读到的"事实"，
    //      于是它敢用对齐指令、敢做 16 字节整块拷贝、敢自动向量化。同一个 bug
    //      在 -O0 下可能一直是好的，开了 -O2 突然段错误。
    //    - 工具层面：-fsanitize=alignment / UBSan 会当场报
    //      "constructor call on misaligned address ... which requires 16 byte alignment"。
    //
    //   std::max_align_t 是**平台最大标量对齐**，标准还保证 operator new / malloc
    //   返回的地址一定是它对齐的。所以 alignas(std::max_align_t) 的语义是
    //   "让这块存储和 new 出来的内存一样干净"，任何标准对齐要求的类型都能安全落位。
    //   代价只是结构体里多了几个填充字节（空间换安全性）：
    //
    //       struct Wrapper {
    //           char      m_tag;
    //           alignas(16) std::byte m_storage[16];   // 插入 15 字节 padding，偏移 16
    //       };
    //
    // 【3】为什么是定长数组而不是柔性数组 / 变长
    //   容量必须在编译期确定，才能让 LogLine 的 sizeof 固定、可以放在栈上，
    //   也才能让 placement new 的目标地址在编译期就对齐好。
    //
    // 注意：std::max_align_t **只覆盖到平台最大标量对齐**。若将来 Impl 里出现
    // alignas(32) / alignas(64) 的成员（SIMD、按 cache line 对齐），这片存储就
    // 不够用了 —— .cpp 里用 static_assert(alignof(Impl) <= alignof(std::max_align_t))
    // 把这种情况堵在编译期；同样地，sizeof 也有对应的 static_assert 防止越界。
    // ------------------------------------------------------------------
    alignas(std::max_align_t) std::byte m_implStorage[LOG_LINE_IMPL_SIZE];

    /**
     * @brief 把字节存储解释成 Impl
     *
     * 私有：`reinterpret_cast` + `std::launder` 这种危险操作只允许出现在这一个地方，
     * 调用方拿不到裸存储，就不可能写出越界 / 未对齐的访问。
     */
    [[nodiscard]] Impl& getImpl() noexcept;
};

/**
 * @brief printf 风格的日志入口（给习惯 `%d` 写法的调用方，或需要接第三方格式化串的场景）
 *
 * 同样是 RAII：函数体里只有一个 `LogLine` 对象，**所有 return 路径都由它的析构函数提交**，
 * 因此 `format == nullptr`、格式化失败这些分支也不会漏日志。
 *
 * 格式化策略（常见路径**只格式化一次**）：
 *  1. 先拿栈上 `PRINTF_FORMAT_INLINE_CAPACITY` 字节的数组直接 `vsnprintf`；
 *  2. `vsnprintf` 返回的是"**不含结尾 '\0'** 的所需长度"：
 *     - 小于缓冲区大小 → 已经完整写进去了，直接 `stream().write(...)`；
 *     - 否则说明被截断，按所需长度开 `std::vector` 再格式化一次（用 `va_copy` 的副本，
 *       因为第一趟已经把原始 `va_list` 消费掉）；
 *  3. 返回值 `< 0` 表示编码错误等格式化失败 → 写占位符而不是静默丢弃。
 *
 * 失败时的占位符（保留"这里本该有条日志"的可观测性）：
 *  - `<null-format>`：传了空格式串；
 *  - `<format-error>`：`vsnprintf` 报错。
 *
 * 注意：本函数**不是 `noexcept`**。溢出路径要 `std::vector` 分配，极端内存不足时
 * `bad_alloc` 会抛给调用方；若要求完全隔离（规范 7.1），需在函数体外层整体包
 * `try/catch` 并标 `noexcept`，代价是再也拿不到"格式化失败"这一信息。
 *
 * @param logger    目标日志器
 * @param level     日志等级
 * @param line      行号，通常由宏传 `__LINE__`
 * @param file_name 文件名，通常由宏传 `__FILE__`
 * @param format    printf 格式串
 */
CAS_API void LogPrintf(Logger& logger, LogLevel::Level level, u32 line,
                       std::string_view file_name, const char* format, ...);

} // namespace Detail
} // namespace CAS

#endif // CAS_INCLUDE_CAS_LOGGER_LOG_LINE_H
