# ============================================================
# 手动观测程序：验证线程 ID / 线程名 / UTF-8 线程名截断
#
# 运行后会 sleep 一段时间，期间请在另一个终端观察：
#   top -H -p <pid>
#   ps -T -p <pid>
#   cat /proc/<pid>/task/*/comm
#
# 因为它会阻塞 60 秒以上，所以不注册到 ctest，只作为可执行文件构建。
# ============================================================
add_executable(TestThreadUtils TestThreadUtils.cpp)

target_link_libraries(TestThreadUtils PRIVATE CAS)

# ============================================================
# 手动观测程序：日志前端（LogLine / LogPrintf / CAS_LOG_* 宏）
#
# 运行后会把 5 个等级 x 2 种写法（流式 / printf）以及各种类型的拼接结果
# 同时输出到 stdout 和 <系统临时目录>/test_logger.log，用于人工核对格式。
# 输出内容没有可断言的返回值，所以只构建可执行文件，不注册到 ctest。
# ============================================================
add_executable(TestLog TestLog.cpp)

target_link_libraries(TestLog PRIVATE CAS)
