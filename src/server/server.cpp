// /src/server/server.cpp
#include "CurrentController.h"
#include "ThreadPoolSingleton.h"
#include "flow_control.h"
#include "logging.h"
#include "server_data_handler.h"
#include "utils.h"
#include <arpa/inet.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <cmath> // 对于std::ceil
#include <cstring>
#include <future>
#include <iostream>
#include <netinet/in.h>
#include <numeric> // 包含std::iota
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

void processMiniBatch(const std::vector<int> &imageDataList, const std::pair<int, int> &minibatchMedaData,
                      const std::pair<std::string, uint16_t> &ipPortPair, int currentBatchNumber,
                      int currentEpochNumber, size_t currentWorkerNumber, ImageData *allImageData,
                      CurrentController *currentController)
{
    // 1.建连接
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(ipPortPair.second);
    serv_addr.sin_addr.s_addr = inet_addr(ipPortPair.first.c_str());

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        fprintf(stderr, "Epoch %d: Connection to worker %s failed\n", currentEpochNumber, ipPortPair.first.c_str());
        return;
    }

    // 2.从内存大vector复制小批次
    std::vector<ImageData> loadedImages = loadImages(imageDataList, minibatchMedaData, allImageData);

    // 3.序列化+打上头部+send
    size_t sentBytes = sendMinibatch(loadedImages, sock, currentBatchNumber, currentEpochNumber, currentWorkerNumber,
                                     currentController);
    if (sentBytes == 0)
    {
        fprintf(stderr, "Send to worker %s failed\n", ipPortPair.first.c_str());
    }
    else
    {
        LOG("Finish batch " << currentBatchNumber << " for worker " << currentWorkerNumber << " in epoch "
                            << currentEpochNumber << ", minibatch size: "
                            << minibatchMedaData.second - minibatchMedaData.first << ", bytes sent: " << sentBytes);
    }

    // 4.关闭socket
    close(sock);
}

void sendBatchNumber(int batchNumber, const std::pair<std::string, uint16_t> &ipPortPair, int totalEpochSize)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(ipPortPair.second);
    serv_addr.sin_addr.s_addr = inet_addr(ipPortPair.first.c_str());

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        fprintf(stderr, "Sending batch number, Connection to worker %s failed\n", ipPortPair.first.c_str());
        close(sock);
        return;
    }

    uint32_t network_batchNumber = htonl(batchNumber);
    uint32_t network_totalEpochSize = htonl(totalEpochSize);

    // 发送batchNumber
    int result = send(sock, &network_batchNumber, sizeof(network_batchNumber), 0);
    if (result < 0)
    {
        perror("Send batchNumber failed");
        close(sock);
        return; // Return early to avoid further attempts to send
    }

    // 发送totalEpochSize
    result = send(sock, &network_totalEpochSize, sizeof(network_totalEpochSize), 0);
    if (result < 0)
    {
        perror("Send totalEpochSize failed");
        close(sock);
        return; // Return early to ensure the function exits after handling the error
    }

    close(sock); // Close the socket after successfully sending both numbers
}

void handleClient(int clientSocket, int serverListenFd)
{
    struct timeval start, end;
    gettimeofday(&start, NULL);

    char buffer[1024] = {0};
    int valread = read(clientSocket, buffer, 1024);
    std::string request(buffer, valread);
    if (valread <= 0)
    {
        printf("error message\n");
        return;
    }

    LOG("Received StartSending request.");

    int epochNumber = 0;
    int batchSize = 0;
    int workerNumber = 0;
    std::string ipPortListString;

    deserializeData(buffer, epochNumber, batchSize, workerNumber, ipPortListString);
    LOG("epochNumber: " << epochNumber);
    LOG("batchSize: " << batchSize);
    LOG("workerNumber: " << workerNumber);
    LOG("ipPortList: " << ipPortListString);

    close(clientSocket);
    std::vector<std::pair<std::string, uint16_t>> ipPortList = parseIpPort(ipPortListString);
    if (workerNumber != ipPortList.size())
    {
        printf("worker number != ipPortList length\n");
        return;
    }
    //-----------------------------------数据请求信息处理完毕---------------------------------
    gettimeofday(&end, NULL);
    long seconds = (end.tv_sec - start.tv_sec);
    long micros = ((seconds * 1000000) + end.tv_usec) - (start.tv_usec);
    double dataRequestHandleTime = micros / 1000000.0;

    //-----------------------------------开始准备数据---------------------------------
    gettimeofday(&start, NULL);
    auto realImageData = loadAllImagesToVector(DATASET_PATH);
    int totalSamples = 300 * 1000; // imagenet 固定参数
    std::vector<int> imageDataList(totalSamples);
    // 使用std::iota填充vector，每个元素的值等于其索引
    std::iota(imageDataList.begin(), imageDataList.end(), 0);
    gettimeofday(&end, NULL);
    seconds = (end.tv_sec - start.tv_sec);
    micros = ((seconds * 1000000) + end.tv_usec) - (start.tv_usec);
    double dataLoadTime = micros / 1000000.0;

    // 计算总共需要多少个batch,向下取整，跳过最后一个不完整的batch
    gettimeofday(&start, NULL);
    int totalBatches = totalSamples / (batchSize * workerNumber);
    int epochSizeForEachWorker = totalBatches * batchSize;
    gettimeofday(&end, NULL);
    seconds = (end.tv_sec - start.tv_sec);
    micros = ((seconds * 1000000) + end.tv_usec) - (start.tv_usec);
    double batchInfoCalculateTime = micros / 1000000.0;

    gettimeofday(&start, NULL);
    int workerSequence = 0;
    std::vector<std::unique_ptr<CurrentController>> controllers;

    for (std::pair<std::string, uint16_t> ipPortPair : ipPortList)
    {
        // petentioly parallize
        sendBatchNumber(totalBatches, ipPortPair, epochSizeForEachWorker);
        controllers.emplace_back(std::make_unique<CurrentController>());
        workerSequence++;
    }
    gettimeofday(&end, NULL);
    seconds = (end.tv_sec - start.tv_sec);
    micros = ((seconds * 1000000) + end.tv_usec) - (start.tv_usec);
    double batchInfoSendTime = micros / 1000000.0;

    gettimeofday(&start, NULL);
    std::thread listeningThread(listenForCapacityUpdates, std::ref(controllers), serverListenFd);
    auto &pool = ThreadPoolSingleton::getInstance();
    // 每个epoch
    for (int currentEpochNumber = 0; currentEpochNumber < epochNumber; currentEpochNumber++)
    {
        LOG("Starting epoch " << currentEpochNumber);

        // epoch开始时shuffle
        randomizeImageData(imageDataList);
        LOG("Shuffling finish");

        // 分配本次epoch的数据
        std::vector<std::vector<std::pair<int, int>>> batchDisributeResult =
            distributeBatches(totalSamples, batchSize * workerNumber, workerNumber);

        // 每个batch
        for (size_t currentBatchNumber = 0; currentBatchNumber < batchDisributeResult[0].size(); currentBatchNumber++)
        {
            // 每个worker
            //     std::cout << "Starting batch " << currentBatchNumber << std::endl;
            //     std::cout << std::endl;
            if (batchDisributeResult.size() != ipPortList.size())
            {
                printf("Epoch %d: Meda data error\n", currentEpochNumber);
            }
            for (size_t currentWorkerNumber = 0; currentWorkerNumber < batchDisributeResult.size();
                 currentWorkerNumber++)
            {
                auto currentWorkerBatchMedadata = batchDisributeResult[currentWorkerNumber][currentBatchNumber];
                auto ipPortPair = ipPortList[currentWorkerNumber];

                boost::asio::post(pool, [&imageDataList, currentWorkerBatchMedadata, ipPortPair, currentBatchNumber,
                                         currentEpochNumber, currentWorkerNumber, realImageData, &controllers] {
                    CurrentController *currentController = controllers[currentWorkerNumber].get();
                    processMiniBatch(imageDataList, currentWorkerBatchMedadata, ipPortPair, currentBatchNumber,
                                     currentEpochNumber, currentWorkerNumber, realImageData, currentController);
                });
            }
        }
    }
    pool.join();

    gettimeofday(&end, NULL);
    seconds = (end.tv_sec - start.tv_sec);
    micros = ((seconds * 1000000) + end.tv_usec) - (start.tv_usec);
    double dataSendTime = micros / 1000000.0;

    std::cout << std::endl;
    std::cout << "-------------------------------------------" << std::endl;
    std::cout << "Finish job for " << ipPortList[0].first << ":" << ipPortList[0].second << std::endl;
    std::cout << "Data Request Handle Time: " << dataRequestHandleTime << " seconds." << std::endl;
    std::cout << "Data Load Time: " << dataLoadTime << " seconds." << std::endl;
    std::cout << "Batch Info Calculate Time: " << batchInfoCalculateTime << " seconds." << std::endl;
    std::cout << "Batch Info Send Time: " << batchInfoSendTime << " seconds." << std::endl;
    std::cout << "Data Send Time: " << dataSendTime << " seconds." << std::endl;
    std::cout << "-------------------------------------------" << std::endl;
    std::cout << std::endl;
}

int main()
{
    int serverFd, newSocket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // 创建socket文件描述符
    serverFd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // 绑定socket到端口8080
    bind(serverFd, (struct sockaddr *)&address, sizeof(address));
    listen(serverFd, 3);

    LOG("Waiting for connections...");
    newSocket = accept(serverFd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
    handleClient(newSocket, serverFd);
    close(serverFd);

    return 0;
}
