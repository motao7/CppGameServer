# ---------------------------------------------------------------------------
# 自动收集测试源文件
# GLOB 扫描当前目录下所有 .cpp / .cc / .cxx 文件
# CONFIGURE_DEPENDS：每次构建时重新检查目录，新增/删除文件会自动更新，
#                    不需要手动重新运行 cmake
# ---------------------------------------------------------------------------
file(GLOB CAS_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
)

# 对文件列表排序，保证每次生成的测试顺序稳定，便于对比输出
list(SORT CAS_TEST_SOURCES)

# 测试程序可能用到 std::thread / pthread，需要链接线程库
find_package(Threads REQUIRED)

# 所有测试可执行文件统一输出到这个目录，方便集中查找和运行
set(CAS_TEST_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/tests")

# ---------------------------------------------------------------------------
# 遍历每个测试源文件，生成一个同名可执行目标
# ---------------------------------------------------------------------------
foreach(TEST_SOURCE IN LISTS CAS_TEST_SOURCES)

    # 去掉路径和扩展名，得到目标名，比如 foo.cpp -> foo
    get_filename_component(TEST_NAME "${TEST_SOURCE}" NAME_WE)

    # 防止目标和已有目标重名，重名直接报错
    if(TARGET "${TEST_NAME}")
        message(FATAL_ERROR "Cannot create test target '${TEST_NAME}': a target with that name already exists")
    endif()

    # 为当前测试源文件创建可执行目标
    add_executable("${TEST_NAME}" "${TEST_SOURCE}")

    # 链接 CAS 库和线程库
    # CAS 的 PUBLIC 依赖会让测试同时继承头文件路径和 CAS_options（编译选项）
    # PRIVATE 表示这些依赖只用于当前测试目标，不会向外传播
    target_link_libraries("${TEST_NAME}" PRIVATE CAS Threads::Threads)

    # 设置可执行文件输出目录
    set_target_properties("${TEST_NAME}" PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CAS_TEST_OUTPUT_DIRECTORY}"
    )

    # 多配置生成器（如 Visual Studio、Xcode）默认会在输出目录后追加 Debug/Release 子目录
    # 这里把每个配置的输出目录都显式设成同一路径，保持输出路径一致
    foreach(CONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER "${CONFIG}" CONFIG_UPPER)
        set_target_properties("${TEST_NAME}" PROPERTIES
            "RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER}" "${CAS_TEST_OUTPUT_DIRECTORY}"
        )
    endforeach()

    # 把该可执行文件注册为一个 CTest 测试项
    # 运行 ctest 时会执行这个目标，测试名和可执行文件名相同
    add_test(NAME "${TEST_NAME}" COMMAND "${TEST_NAME}")

endforeach()