#ifndef BATCH_DATA_PROCESSOR_H
#define BATCH_DATA_PROCESSOR_H

#include "common_data_handler.h"
#include "utils.h"
#include <arpa/inet.h> // 提供IP地址转换函数
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <map>
#include <mqueue.h>
#include <mutex>
#include <netinet/in.h> // 定义数据结构sockaddr_in
#include <shared_mutex>
#include <sstream>
#include <string>
#include <sys/mman.h>
#include <sys/socket.h> // 提供socket函数及数据结构
#include <sys/stat.h>
#include <sys/types.h> // 提供数据类型，比如 pid_t
#include <thread>
#include <unistd.h> // 提供通用的文件、目录、程序及进
#include <vector>

class BatchDataProcessor
{
  public:
    BatchDataProcessor(int batchNumberPerEpoch, int epochNumber, int rank, std::string server_address);
    ~BatchDataProcessor();
    void put_data_to_buffer(int epoch, int batch, std::vector<char> *data);
    void start_processing();
    void stop_processing();

  private:
    // void process_next_batch();
    bool has_next_batch();
    void submit_batch_to_dali(const std::vector<char> &batch);
    void send_notify_message_to_server(int newAvailableCapacity);
    void continuely_send_message_to_queue();
    bool is_next_batch_written_to_shm();
    void write_shm(int epoch, int batch, std::vector<char> *data);
    void send_shm_name_to_queue(int epoch, int batch);

    std::vector<char> ***data_buffer;
    bool **shm_write_flag;
    std::pair<int, int> nextExpectBatch;
    std::shared_mutex sharedMutex;
    std::condition_variable_any data_cond;
    bool done;
    std::thread processing_thread;
    int batch_number_per_epoch;
    int epoch_number;
    mqd_t mq; // 消息队列的标识符
    std::atomic<int> sharedMemoryCounter;
    int rank;
    std::string server_address;
};

#endif // BATCH_DATA_PROCESSOR_H
