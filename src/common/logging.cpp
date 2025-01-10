#include "logging.h"

// 初始化全局日志开关，默认为开启状态
bool g_log_enabled = true;

// 定义全局互斥锁
std::mutex g_cout_mutex;
