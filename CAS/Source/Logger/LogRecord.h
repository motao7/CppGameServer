#ifndef SRC_LOGGER_LOG_RECORD_H
#define SRC_LOGGER_LOG_RECORD_H

#include <chrono>
#include <cstdint>
#include <source_location>
#include <string_view>

#include <CAS/Logger/LogLevel.h>
#include "CAS/Macros.h"
namespace CAS::Detail 
{

// 单条日志事件的非拥有视图。
// 仅在同步日志调用期间有效；所有 string_view 指向调用方存储
// （线程局部缓冲、字符串字面量、source_location 内部）。
// 跨异步边界前必须拷贝为拥有型记录。
struct LogRecordView 
{
    LogLevel::Level   m_level = LogLevel::Level::LOG_LV_DEBUG;  // Log等级
    std::string_view  m_loggerName;                    // 日志器名字
    std::string_view  m_message;                       // 日志内容

    std::chrono::system_clock::time_point m_timestamp{}; // 墙上时间
    std::chrono::steady_clock::duration   m_elapsed{};   // 起服至今

    std::uint64_t     m_threadId = 0;                  // 线程 ID
    std::uint64_t     m_fiberId  = 0;                  // 协程 ID
    std::string_view  m_threadName;                    // 线程名

    std::string_view  m_fileName;                      // 文件名
    u32 m_line = 0;                                     // 行号
};

}

#endif // SRC_LOGGER_LOG_RECORD_H