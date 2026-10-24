#ifndef SRC_LOGGER_FORMATTER_H
#define SRC_LOGGER_FORMATTER_H

#include <memory>
#include <string>
#include <vector>

#include "CAS/Logger/Buffer.h"
#include "CAS/Logger/BufferConfig.h"
#include "Logger/LogRecord.h"

namespace CAS::Detail 
{

using FormattedRecordBuffer = InlineBuffer<FORMATTED_RECORD_INLINE_CAPACITY>;

/**
 * 大部分函数不能标记noexcept，因为InlineBuffer涉及Vector使用，Vector可能在堆上分配，如果无内存，分配失败
 * 对应的函数有没有编译器生成的异常处理机制->直接杀死程序
 */
class Formatter final
{
public:
    // 面向接口编程， 策略模式
    class FormatItem 
    {
    public:
        virtual ~FormatItem() = default;
        virtual void format(const LogRecordView& record,
                        FormattedRecordBuffer& output) const = 0;
    };

    explicit Formatter(std::string_view pattern);

    void format(const LogRecordView& record,
                FormattedRecordBuffer& output) const;

    [[nodiscard]] const std::string& getPattern() const noexcept
    {
        return m_pattern;
    }

protected:
    int parse();
    void addLiteral(std::string& literal);

private:
    std::string m_pattern;

    // 组合模式
    std::vector<std::unique_ptr<FormatItem>> m_items;
};

}

#endif