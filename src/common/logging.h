#ifndef LOGGING_H
#define LOGGING_H

#include <iostream>
#include <mutex>

// 全局变量来控制日志是否开启
extern bool g_log_enabled;

// 全局互斥锁用于同步cout输出
extern std::mutex g_cout_mutex;

// 宏定义用于输出日志
#define LOG(...)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (g_log_enabled)                                                                                             \
        {                                                                                                              \
            std::lock_guard<std::mutex> lock(g_cout_mutex);                                                            \
            std::cout << __VA_ARGS__ << std::endl;                                                                     \
        }                                                                                                              \
    } while (0)

#endif // LOGGING_H
