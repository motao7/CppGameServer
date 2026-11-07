#ifndef CAS_INCLUDE_CAS_UTILS_STRING_UTILS_H
#define CAS_INCLUDE_CAS_UTILS_STRING_UTILS_H

#include <string>
#include <string_view>

#include <CAS/Export.h>

namespace CAS
{

/**
 * @brief 通用字符串编解码工具（纯静态类）。
 *
 * 项目内部统一使用 UTF-8 的 std::string；
 * 只在和操作系统宽字符 API 交互时才转成 wchar_t：
 *  - Windows：wchar_t 是 UTF-16（2 字节），可能包含代理对
 *  - Linux  ：wchar_t 是 UTF-32（4 字节），一个 wchar_t 就是一个码点
 */
class CAS_API StringUtils final
{
public:
    // 纯静态类：禁止实例化，所有成员都是 static
    StringUtils() = delete;

    /**
     * @brief 宽字符转 UTF-8
     * @param text 宽字符串
     * @return 转换失败（非法编码 / 内存不足）时触发断言并返回空字符串
     */
    [[nodiscard]] static std::string WStringToString(std::wstring_view text) noexcept;

    /**
     * @brief UTF-8 转宽字符
     * @param text UTF-8 字符串
     * @return 转换失败（非法编码 / 内存不足）时触发断言并返回空字符串
     */
    [[nodiscard]] static std::wstring StringToWString(std::string_view text) noexcept;
};

} // namespace CAS

#endif // CAS_INCLUDE_CAS_UTILS_STRING_UTILS_H
