cmake_minimum_required(VERSION 3.25)

# 强制指定 C/C++ 编译器
# 注意：如果 /usr/bin/gcc 或 /usr/bin/g++ 不存在，配置会失败
set(CMAKE_C_COMPILER "/usr/bin/gcc" CACHE FILEPATH "C compiler" FORCE)
set(CMAKE_CXX_COMPILER "/usr/bin/g++" CACHE FILEPATH "C++ compiler" FORCE)

# 使用 C++20 标准
# CMAKE_CXX_STANDARD_REQUIRED ON 表示必须支持 C++20
# CMAKE_CXX_EXTENSIONS ON 表示允许 GNU 扩展，例如 -std=gnu++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# 定义项目：名称 CAS，版本 0.1.0，语言 C++ 和 C
project(CAS VERSION 0.1.0 LANGUAGES CXX C)

# 引入 CMake 标准安装目录变量，例如 CMAKE_INSTALL_BINDIR
include(GNUInstallDirs)

# 引入包配置辅助函数，用于生成 CASConfig.cmake 等文件
include(CMakePackageConfigHelpers)

# 构建选项
option(CAS_BUILD_EXAMPLES "Build Examples" ON)
option(CAS_BUILD_TESTS    "Build Tests"    ON)
option(CAS_BUILD_TOOLS    "Build Tools"    ON)

# 对于单配置生成器（如 Makefile、Ninja），如果没有指定构建类型，默认设为 Debug
# 多配置生成器（如 Visual Studio、Xcode）不需要 CMAKE_BUILD_TYPE
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()

# 给 CMAKE_BUILD_TYPE 提供可选值，方便在 cmake-gui 或 ccmake 中选择
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    Debug
    Release
    RelWithDebInfo
    MinSizeRel
)

message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")

# 查询当前服务器的逻辑 CPU 核心数
cmake_host_system_information(RESULT CPU_NUMS QUERY NUMBER_OF_LOGICAL_CORES)
message(STATUS "CPU NUMS on current server is: ${CPU_NUMS}")

# 计算并行编译线程数：约为逻辑核心数的 1.5 倍
# 注意 CMake 的 math 是整数运算，所以 1.5 倍会向下取整
math(EXPR JOBS "${CPU_NUMS} + ${CPU_NUMS} / 2")

# 设置默认并行构建级别
# CMAKE_BUILD_PARALLEL_LEVEL 可以影响 cmake --build 的默认并行数
set(CMAKE_BUILD_PARALLEL_LEVEL ${JOBS} CACHE STRING "并行编译线程数")

message(STATUS "Compiler default work jobs: ${CMAKE_BUILD_PARALLEL_LEVEL}")

include(Cmake/CASOptions.cmake)

add_subdirectory(CAS)
if(CAS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()
if(CAS_BUILD_TOOLS)
    add_subdirectory(Tools)
endif()