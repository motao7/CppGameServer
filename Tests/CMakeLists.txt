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
