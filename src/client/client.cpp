// /src/client/client.cpp
#include "BatchDataProcessor.h"
#include "ThreadPoolSingleton.h"
#include "client_data_handler.h"
#include "common_data_handler.h"
#include "logging.h"
#include "utils.h"
#include <arpa/inet.h>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <cerrno> // For errno
#include <condition_variable>
#include <cstring> // For strerror
#include <iostream>
#include <map>
#include <memory>
#include <netinet/in.h>
#include <shared_mutex>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
#include <vector>

// #define RECIEVE_PORT 8090

uint16_t startSending(int epochNumber, int batchSize, int workerNumber, const std::string &ipPortList,
                      const std::string &serverIp, int rank)
{
    std::vector<std::pair<std::string, uint16_t>> ipPortVector = parseIpPort(ipPortList);
    if (rank == 0)
    {
        // construct socket
        int sock = socket(AF_INET, SOCK_STREAM, 0);

        // initiate address
        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);
        serv_addr.sin_addr.s_addr = inet_addr(serverIp.c_str());

        // connect to server
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            perror("Connection failed");
            return 0;
        }

        // prepare data to send
        std::vector<char> serializedData = serializeData(epochNumber, batchSize, workerNumber, ipPortList);
        // for (char c : serializedData)
        // {
        //     std::cout << c;
        // }
        // std::cout << std::endl;
        send(sock, serializedData.data(), serializedData.size(), 0);
        LOG("StartSending request sent");

        // 关闭连接
        close(sock);
    }
    return ipPortVector[rank].second;
}

void handleRecieveData(int sock, std::shared_ptr<BatchDataProcessor> processor)
{
    char header[8];
    ssize_t received = recv(sock, header, 8, MSG_WAITALL);
    if (received < 8)
    {
        // 错误处理: 数据没有完全接收
        std::cerr << "Failed to receive the complete header." << std::endl;
        return;
    }

    // 将头信息转换为std::string（如果parseHeader接受的是std::string）
    std::string headerStr(header, 8);

    int workerNumber, epochNumber, batchNumber, dataLength;
    // 解析头信息
    parseHeader(headerStr, workerNumber, epochNumber, batchNumber, dataLength);
    //     printf("workerNumber: %d\n", workerNumber);
    //     printf("epochNumber: %d\n", epochNumber);
    //     printf("batchNumber: %d\n", batchNumber);
    //     printf("dataLength: %d bytes\n", dataLength);

    std::vector<char> *data = new std::vector<char>(dataLength);
    size_t totalReceived = 0;
    while (totalReceived < dataLength)
    {
        ssize_t received = recv(sock, &((*data)[totalReceived]), dataLength - totalReceived, MSG_WAITALL);
        if (received == 0)
        {
            // 连接已经被远端关闭
            std::cerr << "Connection closed by peer." << std::endl;
            break;
        }
        else if (received < 0)
        {
            // 发生了错误
            std::cerr << "Failed to receive data: " << strerror(errno) << std::endl;
            break;
        }
        totalReceived += received;
    }

    if (totalReceived < dataLength)
    {
        // 未能接收到全部数据
        std::cerr << "Failed to receive the complete data." << std::endl;
        delete data;
        return;
    }
    //     std::cout << "finish recieve " << totalReceived << "bytes, epoch " << epochNumber << ", batch " <<
    //     batchNumber
    //               << std::endl;
    //     std::cout << "data vector length = " << data.size() << std::endl;
    //     // printHex(data);
    //     std::vector<ImageData> batch_data = deserialize_batch(data);
    //     std::cout << "finish deserialize epoch " << epochNumber << ", batch " << batchNumber
    //               << ", batch size: " << batch_data.size() << std::endl;

    processor->put_data_to_buffer(epochNumber, batchNumber, data);
    LOG("Recieved and deliver to buffer epoch: " << epochNumber << ", batch: " << batchNumber);

    //     std::shared_lock<std::shared_mutex> lock(sharedMutex);
    //     data_buffer[epochNumber][batchNumber] = batch_data;
    //     data_cond.notify_one(); // 通知消费者有新数据

    close(sock); // 关闭客户端套接字
}

int main(int argc, char *argv[])
{
    if (argc != 7)
    { // 检查参数数量是否正确
        std::cerr << "Usage: " << argv[0] << " <epochNumber> <batchSize> <workerNumber> <rank> <ipPortList> <serverIP>"
                  << std::endl;
        return 1;
    }
    int epochNumber = std::stoi(argv[1]);
    int batchSize = std::stoi(argv[2]);
    int workerNumber = std::stoi(argv[3]);
    int rank = std::stoi(argv[4]);
    std::string ipPortListStr = argv[5];
    std::string serverIP = argv[6];

    LOG("Start Sending request.");

    uint16_t recievePort = startSending(epochNumber, batchSize, workerNumber, ipPortListStr, serverIP, rank);

    int serverFd, newSocket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // 创建套接字
    if ((serverFd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        std::cerr << "Socket failed" << std::endl;
        return -1;
    }

    // 设置套接字选项
    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        std::cerr << "Setsockopt failed" << std::endl;
        return -1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // 监听来自任何地址的连接
    address.sin_port = htons(recievePort);

    // 绑定套接字到端口
    if (bind(serverFd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        std::cerr << "Bind failed: " << strerror(errno) << std::endl;
        return -1;
    }

    // 开始监听
    if (listen(serverFd, 3) < 0)
    {
        std::cerr << "Listen failed" << std::endl;
        return -1;
    }

    int batchNumberPerEpoch = 0;
    int totalEpochSize = 0;

    // recieve total batch number per epoch
    if ((newSocket = accept(serverFd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) >= 0)
    {
        LOG("Recieved server response");
        uint32_t network_batchNumber;
        uint32_t network_totalEpochSize;
        ssize_t bytesRead = recv(newSocket, &network_batchNumber, sizeof(network_batchNumber), 0);
        if (bytesRead < 0)
        {
            perror("recv batchNumber failed");
            close(newSocket);
            return -1;
        }
        else if (bytesRead == 0)
        {
            printf("Client closed the connection\n");
            close(newSocket);
            return 0;
        }

        bytesRead = recv(newSocket, &network_totalEpochSize, sizeof(network_totalEpochSize), 0);
        if (bytesRead < 0)
        {
            perror("recv totalEpochSize failed");
            close(newSocket);
            return -1;
        }
        else if (bytesRead == 0)
        {
            printf("Client closed the connection\n");
            close(newSocket);
            return 0;
        }

        // 将网络字节序的batchNumber转换为主机字节序
        batchNumberPerEpoch = static_cast<int>(ntohl(network_batchNumber));
        totalEpochSize = static_cast<int>(ntohl(network_totalEpochSize));
        writeIntToFile("shared_data.txt", totalEpochSize);

        LOG("Start recieveing data, total epoch: " << epochNumber << ", batches per epoch: " << batchNumberPerEpoch);
        // 关闭新的socket连接
        close(newSocket);
    }

    auto processor = std::make_shared<BatchDataProcessor>(batchNumberPerEpoch, epochNumber, rank, serverIP);
    int killCounter = 0;
    struct timeval start, end;
    auto &pool = ThreadPoolSingleton::getInstance();
    //  接受连接请求
    while (killCounter != batchNumberPerEpoch * epochNumber &&
           (newSocket = accept(serverFd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) >= 0)
    {
        killCounter++;
        if (killCounter == 1)
        {
            gettimeofday(&start, NULL);
        }
        boost::asio::post(pool, [newSocket, processor] { handleRecieveData(newSocket, processor); });
    }

    pool.join();
    // processor->stop_processing();

    LOG("Client recieve and write shared memory job done, total epoch: " << epochNumber);

    gettimeofday(&end, NULL);
    long seconds = (end.tv_sec - start.tv_sec);
    long micros = ((seconds * 1000000) + end.tv_usec) - (start.tv_usec);
    double dataRequestHandleTime = micros / 1000000.0;
    std::cout << "Total Data Time: " << dataRequestHandleTime << " seconds." << std::endl;
    return 0;
}
