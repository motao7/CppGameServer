// ============================================================
// 日志前端冒烟测试（手动观测，不注册到 ctest）
//
// 验证的东西：
//   1. 五个等级在流式（CAS_LOG_*）与 printf（CAS_LOG_FMT_*）两种写法下都能正常输出；
//   2. LogLine 的各种拼接类型（char / int / C 字符串 / std::string / std::string_view）
//      都能走通，含多字节 UTF-8；
//   3. 同一个 logger 挂多个 appender（stdout + 文件）能同时收到；
//   4. 应用级快捷方式宏（`LOG_INFO <<`，隐含某个全局 logger）能被复刻。
//
// 为什么是"手动观测"而不是自动化测试：
// 输出内容是给人看的日志文本，没有可断言的返回值；这里也不校验文件内容，
// 只保证"跑起来不崩、该出的都出了"。要断言，应该对 Formatter 单测而不是这里。
//
// 输出位置：
//   - stdout；
//   - <系统临时目录>/test_logger.log（Linux 下是 /tmp/test_logger.log）。
// ============================================================

#include <CAS/Logger/Log.h>
#include <CAS/Logger/Logger.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

// ============================================================
// 应用级快捷方式（仅为演示宏的用法）
//
// 老师的头文件末尾有一组 `#define LOG_INFO PANS_LOG_INFO(g_logger)`，
// 依赖一个名为 g_logger 的日志器。这类"隐含默认 logger"的宏属于
// **游戏/服务器层**，不该放进基础库（库不该知道业务把 logger 存在哪个全局变量里），
// 所以这里在测试文件里复刻一份，用来验证这种写法确实可行。
//
// 关键点：宏只是文本替换，`g_logger` 在**使用处**解析，
// 所以下面 main 里的局部变量 g_logger 也能适配这个宏。
// ============================================================
#define LOG_INFO CAS_LOG_INFO(g_logger)

int main()
{
    // ------------------------------------------------------------
    // 1. 准备输出文件
    //
    // 放系统临时目录：测试不该往仓库里写文件（.gitignore 也管不到随机文件名）。
    // ------------------------------------------------------------
    const std::filesystem::path output_path =
        std::filesystem::temp_directory_path() / "test_logger.log";

    // 先清空上一次运行残留的内容：
    // 构造 ofstream 时用 trunc 会立刻把文件截断为 0 字节，出了作用域就析构关闭。
    // （后面 FileAppender 会自己再打开一次，这里只是为了从干净状态开始观察。）
    {
        std::ofstream truncate_only(output_path, std::ios::binary | std::ios::trunc);
    }

    // ------------------------------------------------------------
    // 2. 造一个 logger，并挂两个 sink
    //
    // 直接用 make_shared<Logger> 而不是 GetLogger("test")：
    // 这里要的是一个可丢弃的独立实例，不污染全局注册表。
    // 两个 appender 都挂上，用来验证"一条日志广播给多个输出端"。
    // ------------------------------------------------------------
    auto logger = std::make_shared<CAS::Logger>("test");
    logger->addAppender(CAS::MakeStdoutAppender());               // 控制台
    logger->addAppender(CAS::MakeFileAppender(output_path.string()));   // 文件

    // ------------------------------------------------------------
    // 3. 五个等级 —— 流式写法
    //
    // CAS_LOG_XXX(logger) 展开成 if / else if / else 链，末尾是
    // LogLine(...).stream()，所以后面能直接接 <<。
    // 每条日志在**分号处**由 LogLine 析构提交（见 CAS/Logger/LogLine.h）。
    // ------------------------------------------------------------
    CAS_LOG_DEBUG(logger) << "debug message";
    CAS_LOG_INFO(logger) << "info message";
    CAS_LOG_WARN(logger) << "warn message";
    CAS_LOG_ERROR(logger) << "error message";
    CAS_LOG_FATAL(logger) << "fatal message";

    std::cout << "----------------------------------------" << std::endl;

    // ------------------------------------------------------------
    // 4. 五个等级 —— printf 写法
    //
    // 走 LogPrintf：宏展开后直接是一次函数调用（没有 .stream()），
    // 函数内部自己构造 LogLine 并在返回时由析构提交。
    // 注意这里**没有可变参数**：CAS_LOG_FMT_* 依靠 C++20 的 __VA_OPT__
    // 处理"只有格式串"的情况，不会留下多余的逗号。
    // ------------------------------------------------------------
    CAS_LOG_FMT_DEBUG(logger, "debug message");
    CAS_LOG_FMT_INFO(logger, "info message");
    CAS_LOG_FMT_WARN(logger, "warn message");
    CAS_LOG_FMT_ERROR(logger, "error message");
    CAS_LOG_FMT_FATAL(logger, "fatal message");

    // ------------------------------------------------------------
    // 5. 各种类型的拼接 + printf 的类型要求
    // ------------------------------------------------------------
    char c = 'a';
    int i = 42;
    const char msg1[] = "hello";                 // char[6]
    const std::string msg2 = "world";            // std::string
    const std::string_view msg3 = "pans 磐石";    // string_view，含多字节 UTF-8

    // 复刻"全局 logger"快捷方式：故意用 g_ 前缀，让 LOG_INFO 宏能解析到它
    auto g_logger = logger;

    std::cout << "----------------------------------------" << std::endl;

    // 流式：std::ostream 有 int / char / const char* / std::string / std::string_view
    // 的现成重载，直接 << 即可，不用管类型。
    LOG_INFO << "c=" << c << " i=" << i
             << " msg1: " << msg1 << " msg2: " << msg2 << " msg3: " << msg3;

    // printf：类型必须与格式串严格对应，否则是 UB（不是"可能出错"，是未定义行为）。
    //   %c ← char（会被默认实参提升成 int，printf 内部再转回来）
    //   %d ← int
    //   %s ← **const char\***
    // 所以 std::string 必须 .c_str()，std::string_view 必须先转成 std::string
    // 再取 .c_str()——string_view 不保证以 '\0' 结尾，直接传 .data() 会越界读。
    CAS_LOG_FMT_INFO(logger, "c=%c i=%d msg1: %s msg2: %s msg3: %s",
                     c, i, msg1, msg2.c_str(), std::string(msg3).c_str());

    // ------------------------------------------------------------
    // 6. 收尾
    //
    // appender 内部可能带缓冲，进程正常退出时不一定保证刷干净，
    // 显式 flush 一次，保证临时目录里的日志文件内容完整可读。
    // ------------------------------------------------------------
    logger->flush();

    // 注意：std::filesystem::path 的 operator<< 会给路径加引号，所以要 .string()
    std::cout << "log file: " << output_path.string() << std::endl;
    return 0;
}
