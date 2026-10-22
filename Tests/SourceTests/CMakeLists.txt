set(SOURCE_TEST_NAME Test_Log_Buffer)
set(SOURCE_TEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/Test_Log_Buffer.cpp")

add_executable("${SOURCE_TEST_NAME}" "${SOURCE_TEST_SOURCE}")

target_link_libraries("${SOURCE_TEST_NAME}" PRIVATE
    CAS
    Threads::Threads
)

target_include_directories("${SOURCE_TEST_NAME}" PRIVATE
    "${PROJECT_SOURCE_DIR}/CAS/Include/CAS"
)

# 设置默认（单配置生成器）输出目录
set_target_properties("${SOURCE_TEST_NAME}" PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CAS_TEST_OUTPUT_DIRECTORY}"
)

# 多配置生成器（Visual Studio / Xcode / Ninja Multi-Config）：
# 每个配置（Debug/Release/...）有独立输出目录
foreach(CONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER "${CONFIG}" CONFIG_UPPER)
    set_target_properties("${SOURCE_TEST_NAME}" PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER}" "${CAS_TEST_OUTPUT_DIRECTORY}"
    )
endforeach()

# 注册为 CTest 测试：ctest 时运行 test_log_buffer
add_test(NAME "${SOURCE_TEST_NAME}" COMMAND "${SOURCE_TEST_NAME}")