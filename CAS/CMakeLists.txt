# ============================================================
# 收集源文件和头文件
# CONFIGURE_DEPENDS 让 CMake 在重新配置时自动检测新增/删除的文件
# 注意：*.cpp 和 *.cc 之间要有空格，*.h 和 *.hpp 之间也要有空格
# ============================================================
file(GLOB_RECURSE CAS_SOURCES CONFIGURE_DEPENDS
    *.cpp
    *.cc
)
file(GLOB_RECURSE CAS_HEADERS CONFIGURE_DEPENDS
    *.h
    *.hpp
)

# ============================================================
# 创建静态库 CAS
# 变量引用必须使用 ${}，不能写成 S(...) 或 S{...}
# ============================================================
add_library(CAS STATIC
    ${CAS_SOURCES}
    ${CAS_HEADERS}
)

# ============================================================
# 设置头文件搜索路径
# PUBLIC 部分会传递给链接 CAS 的目标
# BUILD_INTERFACE 表示构建时使用的路径
# INSTALL_INTERFACE 表示安装后使用的路径
# ============================================================
target_include_directories(CAS
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ============================================================
# 链接接口库，继承所有编译/链接选项
# 注意：这里写的是 CAS_options，如果实际接口库名是 pans_options，
# 请改为 pans_options，并确保它已经定义。
# ============================================================
target_link_libraries(CAS PUBLIC CAS_options)

# ============================================================
# 主库可能需要更严格的警告
# 对 GNU 和 Clang 编译器，将警告视为错误
# 生成器表达式语法：$<$<条件>:选项>
# ============================================================
target_compile_options(CAS PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Werror>
)

# ============================================================
# 安装规则：安装库文件
# EXPORT CASTargets 用于导出目标，供其他项目 find_package 使用
# 变量引用必须用 ${}，且括号要匹配
# ============================================================
install(TARGETS CAS
    EXPORT CASTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ============================================================
# 安装头文件
# 注意：变量名是 CAS_HEADERS，不是 CAS HEADERS
# 安装到 ${CMAKE_INSTALL_INCLUDEDIR}/CAS 目录下
# ============================================================
install(FILES ${CAS_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/CAS
)

# 通常还需要安装导出的目标文件，例如：
# install(EXPORT CASTargets
#     FILE CASTargets.cmake
#     NAMESPACE CAS::
#     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CAS
# )