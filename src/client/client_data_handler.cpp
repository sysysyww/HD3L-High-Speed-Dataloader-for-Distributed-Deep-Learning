// /src/server/server.cpp
#include "common_data_handler.h"
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

void parseHeader(const std::string &header, int &workerNumber, int &epochNumber, int &batchNumber, int &dataLength)
{
    if (header.size() < 8)
    {
        // Error handling: The header is not of expected size.
        return;
    }

    workerNumber = static_cast<unsigned char>(header[0]);
    epochNumber = static_cast<unsigned char>(header[1]);
    batchNumber = (static_cast<unsigned char>(header[2]) << 8) + static_cast<unsigned char>(header[3]);
    dataLength = (static_cast<unsigned char>(header[4]) << 24) + (static_cast<unsigned char>(header[5]) << 16) +
                 (static_cast<unsigned char>(header[6]) << 8) + static_cast<unsigned char>(header[7]);
}

std::vector<ImageData> deserialize_batch(const std::vector<char> &serialized_batch)
{
    std::vector<ImageData> batch_data;
    const char *dataPtr = serialized_batch.data();
    const size_t totalSize = serialized_batch.size();
    const char *endPtr = dataPtr + totalSize; // 计算结束位置

    while (dataPtr < endPtr)
    {
        ImageData image;
        const char *nextPtr = image.deserialize(dataPtr, endPtr - dataPtr);
        if (nextPtr == nullptr)
        {
            break; // 反序列化失败或数据结束
        }
        batch_data.push_back(image);
        dataPtr = nextPtr; // 更新指针位置继续反序列化下一个ImageData
    }

    return batch_data;
}

// int main()
// {
//     int serverFd, newSocket;
//     struct sockaddr_in address;
//     int opt = 1;
//     int addrlen = sizeof(address);

//     // 创建socket文件描述符
//     serverFd = socket(AF_INET, SOCK_STREAM, 0);
//     setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
//     address.sin_family = AF_INET;
//     address.sin_addr.s_addr = INADDR_ANY;
//     address.sin_port = htons(PORT);

//     // 绑定socket到端口8080
//     bind(serverFd, (struct sockaddr *)&address, sizeof(address));
//     listen(serverFd, 3);

//     std::cout << "Waiting for connections..." << std::endl;
//     newSocket = accept(serverFd, (struct sockaddr *)&address, (socklen_t *)&addrlen);

//     char header[8];
//     ssize_t received = recv(newSocket, header, 8, MSG_WAITALL);
//     if (received < 8)
//     {
//         // 错误处理: 数据没有完全接收
//         std::cerr << "Failed to receive the complete header." << std::endl;
//         return -1;
//     }

//     // 将头信息转换为std::string（如果parseHeader接受的是std::string）
//     std::string headerStr(header, 8);

//     int workerNumber, epochNumber, batchNumber, dataLength;
//     // 解析头信息
//     parseHeader(headerStr, workerNumber, epochNumber, batchNumber, dataLength);

//     printf("workerNumber: %d\n", workerNumber);
//     printf("epochNumber: %d\n", epochNumber);
//     printf("batchNumber: %d\n", batchNumber);
//     printf("dataLength: %d bytes\n", dataLength);

//     std::vector<char> data(dataLength);
//     size_t totalReceived = 0;
//     while (totalReceived < dataLength)
//     {
//         ssize_t received = recv(newSocket, data.data() + totalReceived, dataLength - totalReceived, MSG_WAITALL);
//         if (received == 0)
//         {
//             // 连接已经被远端关闭
//             std::cerr << "Connection closed by peer." << std::endl;
//             break;
//         }
//         else if (received < 0)
//         {
//             // 发生了错误
//             std::cerr << "Failed to receive data: " << strerror(errno) << std::endl;
//             break;
//         }
//         totalReceived += received;
//     }

//     if (totalReceived < dataLength)
//     {
//         // 未能接收到全部数据
//         std::cerr << "Failed to receive the complete data." << std::endl;
//         return -1;
//     }
//     // 成功接收到所有数据
//     close(serverFd);
//     close(newSocket);
//     return 0;
// }
