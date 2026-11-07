#ifndef CAS_INCLUDE_CAS_LOGGER_LOG_H
#define CAS_INCLUDE_CAS_LOGGER_LOG_H

#include <CAS/Export.h>
#include <CAS/Logger/LogLevel.h>
#include <CAS/Logger/LogLine.h>
#include <CAS/Logger/Logger.hpp>

/**
 * 日志宏（业务代码唯一应该直接使用的前端入口）
 *
 * @code
 * // 流式拼接（推荐，零 printf 解析开销）
 * CAS_LOG_INFO(CAS_LOG_NAME("net")) << "connect ok, fd = " << fd;
 *
 * // printf 风格（需要接第三方格式串、或习惯 %d 写法时）
 * CAS_LOG_FMT_WARN(CAS_LOG_ROOT(), "slow query: %s took %d ms", sql, cost_ms);
 * @endcode
 *
 * ============================================================
 * 一、宏展开成什么（这是整个设计的核心）
 * ============================================================
 *
 * 业务代码写的是：`CAS_LOG_INFO(logger) << a << b;`
 * 宏只占据这行的**开头**，`<< a << b` 是调用方自己写的尾巴，
 * 所以宏必须展开成**一条表达式**，让尾巴能续上去：
 *
 *     if (auto lg = (logger); !lg) {}
 *     else if (const auto lv = level; !lg->shouldLog(lv)) {}
 *     else ::CAS::Detail::LogLine(*lg, lv, __LINE__, __FILE__).stream() << a << b;
 *                                                   ↑ 用户写的尾巴接在这里
 *
 * 最后那个 `LogLine(...)` 是临时对象，在**分号处**析构并提交日志
 * （见 CAS/Logger/LogLine.h 的类注释）。
 *
 * ============================================================
 * 二、为什么是 `if / else if / else`，而不是常见的 `do { } while (0)`
 * ============================================================
 *
 * - `do { } while (0)` 展开成的是**语句**，用户接 `<< a` 会直接编译错误；
 * - `if / else if / else` 链展开后仍是一个**完整的语句**，而最后那个 `else`
 *   正好可以挂住用户的 `<< ...;`，于是整条链在语法上依然是一条语句。
 *
 * 更重要的是：链里**每个 if 都配了 else**，所以不会出现"悬空 else"。
 * 反例（错误的写法，把第一个 if 之外的分支写成独立的 if）：
 *
 *     if (auto lg = (logger); !lg) {}          // 独立的 if
 *     if (const auto lv = level; ...) {}       // 又一个独立的 if
 *     else ...                                 // else 只和第二个 if 配对
 *
 * 这样写有两个致命问题：
 *  1. `if (cond) CAS_LOG(...) << x;` 展开后，外层 `if (cond)` 只管辖第一条语句，
 *     日志会在 cond 为 false 时照样输出；
 *  2. logger 为空时第一个 if 什么也不做，但流程继续走到第二个 if，
 *     里面解引用空指针 → 崩溃。
 *
 * ============================================================
 * 三、为什么先判空、再判等级
 * ============================================================
 *
 * | 判断 | 目的 |
 * |------|------|
 * | `!cas_log_logger` | logger 允许为空（例如尚未初始化的全局 logger），此时**静默丢弃**而不是崩 |
 * | `!shouldLog(level)` | 等级不满足时**连 LogLine 都不构造**，`<< a << b` 里的格式化开销全部省掉 |
 *
 * 第二条很关键：`CAS_LOG_DEBUG(lg) << VeryExpensiveToString(obj);`
 * 在 INFO 等级下，`VeryExpensiveToString` 根本不会被调用。
 *
 * ============================================================
 * 四、为什么宏里要 `auto cas_log_logger = (logger)` 拷一份
 * ============================================================
 *
 * `logger` 可以是 `LoggerPtr`（`std::shared_ptr<Logger>`），也可以直接传 `Logger*`：
 * `auto` 拷贝后用 `bool` 转换判空、用 `*` 取引用，两种都适用。
 * 拷一份也保证在整条表达式执行期间 logger 一定存活（`CAS_LOG_INFO(CAS_LOG_NAME("x"))`
 * 这种传临时的写法才安全）。
 *
 * 代价是 `shared_ptr` 的引用计数会做一次 +1/-1（两次原子操作）。
 * 在意这点开销的地方可以自己先取出 `LoggerPtr` 存进局部变量，再传进来。
 *
 * 变量名统一用 `cas_log_` 前缀：宏在调用方作用域里声明变量，
 * 必须避免和业务变量重名（例如业务里也有个 `logger`）。
 *
 * ============================================================
 * 五、`__VA_OPT__` 是什么
 * ============================================================
 *
 * `CAS_LOG_FMT_INFO(lg, "done")` 这种**没有可变参数**的调用，
 * 如果直接写 `, __VA_ARGS__` 会展开成 `..., "done", )` —— 语法错误。
 * C++20 的 `__VA_OPT__(, )` 表示"`__VA_ARGS__` 非空时才展开这个逗号"，
 * 于是两种调用都能成立：
 *
 *     CAS_LOG_FMT_INFO(lg, "done")            → LogPrintf(..., "done")
 *     CAS_LOG_FMT_INFO(lg, "fd=%d", fd)       → LogPrintf(..., "fd=%d", fd)
 *
 * ⚠️ **整个链路上只能有一处 `__VA_OPT__`**：五个等级的 FMT 宏只负责把
 * `__VA_ARGS__` 原样转发给 `CAS_LOG_FMT_LEVEL`，补逗号的事统一由后者做。
 * 若两层都写 `__VA_OPT__(, ) __VA_ARGS__`，就会展开成
 * `LogPrintf(..., (format), , arg)`，两个连续逗号 → 编译报错。
 *
 * ============================================================
 * 六、注意事项
 * ============================================================
 *
 * - `__LINE__` / `__FILE__` 在**宏使用处**展开，所以日志里记的是业务代码的位置；
 * - 宏是表达式链，**不能**在里面 `return` / `throw` / `break`（它没有自己的作用域块）；
 * - 传 `nullptr` 不会崩，但也不会输出任何内容（这是刻意的降级行为）；
 * - 需要"应用级快捷方式"（如 `LOG_INFO << "x"`，隐含某个全局 logger）时，
 *   应该在**游戏/服务器层的头文件**里再包一层 `#define LOG_INFO CAS_LOG_INFO(g_logger)`，
 *   不要放进这个基础库头文件——基础库不该知道业务存在哪个全局 logger。
 */

// ============================================================
// 通用写日志宏：等级由调用方指定
//
// 展开后是一条 if / else if / else 语句，最后的分支是
// `LogLine(...).stream()`，用于挂住用户写的 `<< a << b`。
// ============================================================
#define CAS_LOG_LEVEL(logger, level)                                                     \
    if (auto cas_log_logger = (logger); !cas_log_logger)                                 \
    {                                                                                    \
    }                                                                                    \
    else if (const auto cas_log_level = (level);                                         \
             !cas_log_logger->shouldLog(cas_log_level))                                  \
    {                                                                                    \
    }                                                                                    \
    else ::CAS::Detail::LogLine(*cas_log_logger, cas_log_level, __LINE__, __FILE__)       \
             .stream()

// ============================================================
// 五个等级：CAS_LOG_DEBUG / INFO / WARN / ERROR / FATAL
//
// 全部复用上面的 CAS_LOG_LEVEL，只把等级固定下来。
// ============================================================
#define CAS_LOG_DEBUG(logger) CAS_LOG_LEVEL((logger), ::CAS::LogLevel::Level::LOG_LV_DEBUG)
#define CAS_LOG_INFO(logger)  CAS_LOG_LEVEL((logger), ::CAS::LogLevel::Level::LOG_LV_INFO)
#define CAS_LOG_WARN(logger)  CAS_LOG_LEVEL((logger), ::CAS::LogLevel::Level::LOG_LV_WARN)
#define CAS_LOG_ERROR(logger) CAS_LOG_LEVEL((logger), ::CAS::LogLevel::Level::LOG_LV_ERROR)
#define CAS_LOG_FATAL(logger) CAS_LOG_LEVEL((logger), ::CAS::LogLevel::Level::LOG_LV_FATAL)

// ============================================================
// printf 风格的通用宏
//
// 结构与 CAS_LOG_LEVEL 完全一致，只是末尾换成 LogPrintf 调用（自带提交）。
// 注意末尾**没有** `.stream()`：LogPrintf 内部会自己构造 LogLine 并在
// 返回时由析构提交，所以它是一次完整的函数调用。
// ============================================================
#define CAS_LOG_FMT_LEVEL(logger, level, format, ...)                                     \
    if (auto cas_log_logger = (logger); !cas_log_logger)                                 \
    {                                                                                    \
    }                                                                                    \
    else if (const auto cas_log_level = (level);                                         \
             !cas_log_logger->shouldLog(cas_log_level))                                  \
    {                                                                                    \
    }                                                                                    \
    else ::CAS::Detail::LogPrintf(*cas_log_logger, cas_log_level, __LINE__, __FILE__,     \
                                  (format) __VA_OPT__(, ) __VA_ARGS__)

// ============================================================
// 五个等级的 printf 版本
//
// 这里**只做转发，不要自己再加 `__VA_OPT__(, )`**：
// 上一层的 `__VA_OPT__` 已经负责补逗号，这里再补一次就会变成
// `LogPrintf(..., (format), , arg)` —— 两个连续逗号，编译报错。
// 可变参数直接 `__VA_ARGS__` 原样透传即可（空的时候它在 FMT_LEVEL 里也是空的）。
// ============================================================
#define CAS_LOG_FMT_DEBUG(logger, format, ...)                                            \
    CAS_LOG_FMT_LEVEL((logger), ::CAS::LogLevel::Level::LOG_LV_DEBUG, (format), __VA_ARGS__)
#define CAS_LOG_FMT_INFO(logger, format, ...)                                             \
    CAS_LOG_FMT_LEVEL((logger), ::CAS::LogLevel::Level::LOG_LV_INFO, (format), __VA_ARGS__)
#define CAS_LOG_FMT_WARN(logger, format, ...)                                             \
    CAS_LOG_FMT_LEVEL((logger), ::CAS::LogLevel::Level::LOG_LV_WARN, (format), __VA_ARGS__)
#define CAS_LOG_FMT_ERROR(logger, format, ...)                                            \
    CAS_LOG_FMT_LEVEL((logger), ::CAS::LogLevel::Level::LOG_LV_ERROR, (format), __VA_ARGS__)
#define CAS_LOG_FMT_FATAL(logger, format, ...)                                            \
    CAS_LOG_FMT_LEVEL((logger), ::CAS::LogLevel::Level::LOG_LV_FATAL, (format), __VA_ARGS__)

// ============================================================
// 取日志器的小helper（避免业务代码到处拼 ::CAS::）
//
//   CAS_LOG_ROOT()          等价于  ::CAS::GetRootLogger()
//   CAS_LOG_NAME("network") 等价于  ::CAS::GetLogger("network")
// ============================================================
#define CAS_LOG_ROOT()      ::CAS::GetRootLogger()
#define CAS_LOG_NAME(name)  ::CAS::GetLogger((name))

#endif // CAS_INCLUDE_CAS_LOGGER_LOG_H
