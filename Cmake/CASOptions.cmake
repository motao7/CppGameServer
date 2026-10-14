# 如果还没有定义 CAS_options 接口库，需要先创建
add_library(CAS_options INTERFACE)

# ============================================================
# 编译器警告与严格性选项：仅对 GNU 和 Clang 启用
# ============================================================
target_compile_options(CAS_options INTERFACE
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wpedantic>
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-fno-strict-aliasing>
)

# ============================================================
# 位置无关代码：Linux 平台下的 GNU/Clang 编译器
# ============================================================
target_compile_options(CAS_options INTERFACE
    $<$<AND:$<PLATFORM_ID:Linux>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:-fPIC>
)

# ============================================================
# 链接选项：Linux 平台下的 GNU/Clang 编译器
# ============================================================
target_link_options(CAS_options INTERFACE
    $<$<AND:$<PLATFORM_ID:Linux>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:-rdynamic>
)

# ============================================================
# 编译宏定义：Debug 配置下定义 CAS_DEBUG
# ============================================================
target_compile_definitions(CAS_options INTERFACE
    $<$<CONFIG:Debug>:CAS_DEBUG>
)

# ============================================================
# Debug 配置的编译选项
# ============================================================
target_compile_options(CAS_options INTERFACE
    $<$<CONFIG:Debug>:-O0>      # 关闭优化，便于调试（注意是字母 O，不是数字 0）
    $<$<CONFIG:Debug>:-g3>      # 生成最多调试信息
    $<$<CONFIG:Debug>:-ggdb>    # 生成 GDB 专用调试信息
)

# ============================================================
# Release / RelWithDebInfo 共用选项
# ============================================================
target_compile_options(CAS_options INTERFACE
    # Release 配置
    $<$<CONFIG:Release>:-O2>                    # 二级优化
    $<$<CONFIG:Release>:-fno-omit-frame-pointer> # 保留帧指针，便于性能分析和栈回溯

    # RelWithDebInfo 配置
    $<$<CONFIG:RelWithDebInfo>:-O2>             # 二级优化
    $<$<CONFIG:RelWithDebInfo>:-g>              # 生成调试信息
    $<$<CONFIG:RelWithDebInfo>:-fno-omit-frame-pointer> # 保留帧指针
)

# 对于 Release 和 RelWithDebInfo，通常还需要定义 NDEBUG 来关闭断言
target_compile_definitions(CAS_options INTERFACE
    $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:NDEBUG>
)

# ============================================================
# 代码覆盖率选项（可选）
# ============================================================
option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)

if(ENABLE_COVERAGE)
    target_compile_options(CAS_options INTERFACE
        $<$<CONFIG:Debug>:--coverage>   # 编译时插入覆盖率插桩
    )
    target_link_options(CAS_options INTERFACE
        $<$<CONFIG:Debug>:--coverage>   # 链接时加入覆盖率运行时
    )
endif()