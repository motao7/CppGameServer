#ifndef CAS_SRC_LOGGER_BUFFER_H
#define CAS_SRC_LOGGER_BUFFER_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <streambuf>
#include <string_view>
#include <vector>

namespace CAS::Detail
{
    // 编译器即可确定数组大小
    template<std::size_t INLINE_CAPACITY>
    class InlineBuffer
    {
    public:
        void append(const char* data,std::size_t size)
        {
            if(size == 0)
            {
                return;
            }

            if(m_overflow.empty())
            {
                if(m_size + size <= INLINE_CAPACITY)
                {
                    std::memcpy(m_inline.data() + m_size, data, size);
                    m_size += size;
                    return;
                }
                else
                {
                    const std::size_t required_capacity = m_size + size;
                    m_overflow.reserve(std::max(INLINE_CAPACITY * 2, required_capacity));
                    m_overflow.insert(m_overflow.end(), m_inline.data(), m_inline.data() + m_size);
                    m_overflow.insert(m_overflow.end(), data, data + size);
                    m_size = m_overflow.size();
                    return;
                }
            }
            else
            {
                m_overflow.insert(m_overflow.end(), data, data + size);
                m_size = m_overflow.size();
            }
        }   
        
        void append(std::string_view value)
        {
            append(value.data(), value.size());
        }

        void append(char value)
        {
            append(&value, 1);
        }

        [[nodiscard]] const char* data() const noexcept
        {
            return m_overflow.empty() ? m_inline.data() : m_overflow.data();
        }

        [[nodiscard]] std::size_t size() const noexcept
        {
            return m_size;
        }

        [[nodiscard]] std::string_view view() const noexcept
        {
            return {data(), size()};
        }

    private:
        std::array<char, INLINE_CAPACITY> m_inline{};   // 短日志
        std::vector<char> m_overflow;                   // 长日志
        std::size_t m_size = 0;
    };

    // SmallStreamBuffer：把 std::ostream 的输出重定向到 InlineBuffer
    // 继承 std::streambuf，重写两个受保护虚函数接管所有写操作
    template<std::size_t INLINE_CAPACITY>
    class SmallStreamBuffer final : public std::streambuf
    {
    public:
        // 构造：绑定一个外部 InlineBuffer（引用，不拥有）
        // explicit：禁止隐式转换；noexcept：只是绑引用，不会失败
        explicit SmallStreamBuffer(InlineBuffer<INLINE_CAPACITY>& buffer) noexcept
            : m_buffer(buffer)
        {}

    protected:
        // ===== 批量写：ostream 写一串字符时调用 =====
        // 触发：os << "字符串"、os.write、格式化数字串
        std::streamsize xsputn(const char* data, std::streamsize size) override
        {
            if (size <= 0)
                return 0;
            m_buffer.append(data, static_cast<std::size_t>(size));
            return size;
        }

        // ===== 单字符写 / 刷新 =====
        // 触发：os << 'c'、os.put、flush、endl、析构
        int_type overflow(int_type character) override
        {
            // 传入 eof → 表示“刷新/结束”，没有字符要写
            // 必须返回“非 eof”值表示成功（不能原样返回 eof，那是失败）
            if (traits_type::eq_int_type(character, traits_type::eof()))
                return traits_type::not_eof(character);

            // 传入普通字符 → to_char_type 转回 char，append 进缓冲
            // 返回该字符（非 eof）表示成功
            m_buffer.append(traits_type::to_char_type(character));
            return character;
        }

    private:
        InlineBuffer<INLINE_CAPACITY>& m_buffer;   // 引用的外部缓冲
    };
}

#endif
