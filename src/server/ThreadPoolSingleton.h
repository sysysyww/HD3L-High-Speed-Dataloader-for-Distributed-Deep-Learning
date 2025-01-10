// ThreadPoolSingleton.h
#ifndef THREAD_POOL_SINGLETON_H
#define THREAD_POOL_SINGLETON_H

#include <boost/asio/thread_pool.hpp>
#include <memory>

class ThreadPoolSingleton
{
  public:
    static boost::asio::thread_pool &getInstance()
    {
        static ThreadPoolSingleton instance(28); // 假设池大小为4
        return instance.pool;
    }

  private:
    boost::asio::thread_pool pool;

    ThreadPoolSingleton(int size) : pool(size)
    {
    }
    // 禁止复制和赋值
    ThreadPoolSingleton(const ThreadPoolSingleton &) = delete;
    ThreadPoolSingleton &operator=(const ThreadPoolSingleton &) = delete;
};

#endif
