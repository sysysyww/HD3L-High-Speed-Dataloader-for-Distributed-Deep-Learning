#include "BatchDataProcessor.h"
#include "logging.h"

BatchDataProcessor::BatchDataProcessor(int batchNumberPerEpoch, int epochNumber, int rank, std::string server_address)
    : nextExpectBatch({0, 0}), done(false), batch_number_per_epoch(batchNumberPerEpoch), epoch_number(epochNumber),
      sharedMemoryCounter(0), rank(rank), server_address(server_address)
{
    struct mq_attr attr;
    attr.mq_flags = 0;     // 阻塞模式
    attr.mq_maxmsg = 2048; // 队列中最大消息数
    attr.mq_msgsize = 256; // 每个消息的最大大小
    attr.mq_curmsgs = 0;   // 当前队列中的消息数（只读，创建时忽略）

    // 创建或打开消息队列
    mq = mq_open("/myQueue", O_CREAT | O_RDWR, 0644, &attr);
    if (mq == (mqd_t)-1)
    {
        perror("mq_open");
        exit(1); // 处理错误情况
    }

    // iniialize data_buffer
    data_buffer = new std::vector<char> **[epochNumber];
    shm_write_flag = new bool *[epochNumber];
    for (int i = 0; i < epochNumber; ++i)
    {
        data_buffer[i] = new std::vector<char> *[batchNumberPerEpoch];
        shm_write_flag[i] = new bool[batchNumberPerEpoch];
        for (int j = 0; j < batchNumberPerEpoch; ++j)
        {
            data_buffer[i][j] = nullptr; // 初始化为nullptr，可以稍后根据需要分配
            shm_write_flag[i][j] = false;
        }
    }

    start_processing();
}

BatchDataProcessor::~BatchDataProcessor()
{
    if (processing_thread.joinable())
    {
        stop_processing();
    }
    while (1)
    {
        struct mq_attr attr;
        mq_getattr(mq, &attr);
        if (attr.mq_curmsgs == 0)
        {
            // 关闭消息队列
            mq_close(mq);
            // 删除消息队列
            mq_unlink("/myQueue");
            break;
        }
    }
}

void BatchDataProcessor::put_data_to_buffer(int epoch, int batch, std::vector<char> *data)
{
    // std::shared_lock<std::shared_mutex> lock(sharedMutex);
    data_buffer[epoch][batch] = data;
    write_shm(epoch, batch, data);
    // data_cond.notify_one(); // 通知消费者有新数据
}

void BatchDataProcessor::send_notify_message_to_server(int newAvailableCapacity)
{
    int sockfd;
    struct sockaddr_in servaddr;

    // 创建socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // 设置服务器地址和端口
    servaddr.sin_family = AF_INET;   // IPv4
    servaddr.sin_port = htons(PORT); // 端口号
    servaddr.sin_addr.s_addr = inet_addr(this->server_address.c_str());

    // 连接到服务器
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("connect failed");
        exit(EXIT_FAILURE);
    }

    // 准备要发送的数据
    int data[2];
    data[0] = this->rank;
    data[1] = newAvailableCapacity;
    // 发送数据
    if (write(sockfd, data, sizeof(data)) < 0)
    {
        perror("write to socket failed");
        exit(EXIT_FAILURE);
    }

    printf("Notify server new available capacity %d\n", newAvailableCapacity);

    // 关闭socket
    close(sockfd);
}

// // 上交线程，循环检索大vector里是否有下一批次，然后写入共享内存
// void BatchDataProcessor::process_next_batch()
// {
//     std::unique_lock<std::shared_mutex> lock(sharedMutex);
//     while (!done)
//     {
//         data_cond.wait(lock, [this] { return done || has_next_batch(); });
//         if (has_next_batch())
//         {
//             int epochNumber = nextExpectBatch.first;
//             int batchNumber = nextExpectBatch.second;
//             auto batch = data_buffer[nextExpectBatch.first][nextExpectBatch.second];
//             data_buffer[nextExpectBatch.first].erase(nextExpectBatch.second);

//             nextExpectBatch.second++;
//             if (nextExpectBatch.second >= this->batch_number_per_epoch)
//             {
//                 nextExpectBatch.first++;
//                 nextExpectBatch.second = 0;
//                 if (nextExpectBatch.first >= this->epoch_number)
//                 {
//                     done = true;
//                     std::cout << "reallyYYYYYYYYYYYYYYYYYY?" << nextExpectBatch.first << " == " << this->epoch_number
//                               << std::endl;
//                 }
//             }
//             lock.unlock();
//             submit_batch_to_dali(batch);

//             std::cout << std::endl;
//             std::cout << "-------------------------------------------" << std::endl;
//             std::cout << "Write to shared memory epoch: " << epochNumber << ", batch: " << batchNumber
//                       << ", bytes: " << batch.size() << std::endl;
//             std::cout << "-------------------------------------------" << std::endl;
//             std::cout << std::endl;
//             // send_notify_message_to_server(batch.size());
//             lock.lock();
//         }
//     }
// }

void BatchDataProcessor::continuely_send_message_to_queue()
{
    while (!done)
    {
        if (is_next_batch_written_to_shm())
        {
            int epochNumber = nextExpectBatch.first;
            int batchNumber = nextExpectBatch.second;
            send_shm_name_to_queue(epochNumber, batchNumber);
            nextExpectBatch.second++;
            if (nextExpectBatch.second >= this->batch_number_per_epoch)
            {
                nextExpectBatch.first++;
                nextExpectBatch.second = 0;
                if (nextExpectBatch.first >= this->epoch_number)
                {
                    done = true;
                }
            }
        }
    }
}

bool BatchDataProcessor::has_next_batch()
{
    // std::cout << "expect next: epoch " << nextExpectBatch.first << " batch " << nextExpectBatch.second << std::endl;

    return data_buffer[nextExpectBatch.first][nextExpectBatch.second] != nullptr;
}

bool BatchDataProcessor::is_next_batch_written_to_shm()
{
    return shm_write_flag[nextExpectBatch.first][nextExpectBatch.second];
}

void BatchDataProcessor::write_shm(int epoch, int batch, std::vector<char> *data)
{
    std::stringstream ss;
    ss << "/shm-" << epoch << "-" << batch;
    std::string shmName = ss.str();

    // 创建共享内存
    int shm_fd = shm_open(shmName.c_str(), O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, data->size());
    void *mmap_ptr = mmap(0, data->size(), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    // 将数据复制到共享内存
    std::memcpy(mmap_ptr, data->data(), data->size());

    // release mem
    delete data_buffer[epoch][batch];
    data_buffer[epoch][batch] = nullptr;

    shm_write_flag[epoch][batch] = true;

    LOG("Write to shared memory epoch: " << epoch << ", batch: " << batch << ", bytes: " << data->size());
}

void BatchDataProcessor::send_shm_name_to_queue(int epoch, int batch)
{
    std::stringstream ss;
    ss << "/shm-" << epoch << "-" << batch;
    std::string shmName = ss.str();

    mqd_t mq = mq_open("/myQueue", O_WRONLY);
    mq_send(mq, shmName.c_str(), shmName.size(), 0);
    mq_close(mq);

    LOG("Sent shm name to queue: " << epoch << ", batch: " << batch);
}

void BatchDataProcessor::submit_batch_to_dali(const std::vector<char> &batch)
{
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    int counter = sharedMemoryCounter.fetch_add(1); // 原子递增

    std::stringstream ss;
    ss << "/shm-" << milliseconds << "-" << counter; // 结合时间戳和计数器生成名称
    std::string shmName = ss.str();

    // 创建共享内存
    int shm_fd = shm_open(shmName.c_str(), O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, batch.size());
    void *mmap_ptr = mmap(0, batch.size(), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    // 将数据复制到共享内存
    std::memcpy(mmap_ptr, batch.data(), batch.size());

    // 将共享内存的名称发送到消息队列
    mqd_t mq = mq_open("/myQueue", O_WRONLY);
    mq_send(mq, shmName.c_str(), shmName.size(), 0);
    mq_close(mq);

    // 资源清理交给消费者完成
    // std::cout << "Data written to shared memory: " << shmName << std::endl;
}

void BatchDataProcessor::start_processing()
{
    processing_thread = std::thread(&BatchDataProcessor::continuely_send_message_to_queue, this);
}

void BatchDataProcessor::stop_processing()
{
    //     {
    //         std::unique_lock<std::shared_mutex> lock(sharedMutex);
    //         done = true;
    //     }
    data_cond.notify_one();
    processing_thread.join();
}
