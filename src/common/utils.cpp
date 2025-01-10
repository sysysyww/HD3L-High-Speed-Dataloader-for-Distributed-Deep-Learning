#include "utils.h"
#include <arpa/inet.h>
#include <cerrno> // For errno
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

std::vector<std::pair<std::string, uint16_t>> parseIpPort(const std::string &input)
{
    std::vector<std::pair<std::string, uint16_t>> result;
    std::istringstream iss(input);
    std::string token;

    while (std::getline(iss, token, ';'))
    {
        auto colonPos = token.find(':');
        if (colonPos != std::string::npos)
        {
            std::string ip = token.substr(0, colonPos);
            uint16_t port;
            try
            {
                port = static_cast<uint16_t>(std::stoi(token.substr(colonPos + 1)));
            }
            catch (const std::invalid_argument &e)
            {
                std::cerr << "port parse error, stoi error: " << e.what() << std::endl;
                port = 8090;
            }
            result.emplace_back(ip, port);
        }
    }

    return result;
}

std::vector<char> serializeData(int epochNumber, int batchSize, int workerNumber, const std::string &ipPortList)
{
    // 首先计算总数据大小
    size_t totalSize =
        sizeof(epochNumber) + sizeof(batchSize) + sizeof(workerNumber) + sizeof(size_t) + ipPortList.size();

    // 创建一个足够大的字节向量来存储所有数据
    std::vector<char> serializedData(totalSize);

    // 一个指针，用来追踪在字节向量中的位置
    char *ptr = serializedData.data();

    // 复制 epochNumber 到字节向量
    std::memcpy(ptr, &epochNumber, sizeof(epochNumber));
    ptr += sizeof(epochNumber);

    // 复制 batchSize 到字节向量
    std::memcpy(ptr, &batchSize, sizeof(batchSize));
    ptr += sizeof(batchSize);

    // 复制 workerNumber 到字节向量
    std::memcpy(ptr, &workerNumber, sizeof(workerNumber));
    ptr += sizeof(workerNumber);

    // 复制 ipPortList 的大小和数据到字节向量
    size_t ipPortListSize = ipPortList.size();
    std::memcpy(ptr, &ipPortListSize, sizeof(ipPortListSize));
    ptr += sizeof(ipPortListSize);
    std::memcpy(ptr, ipPortList.data(), ipPortListSize);

    return serializedData;
}

void deserializeData(const char *serializedData, int &epochNumber, int &batchSize, int &workerNumber,
                     std::string &ipPortList)
{
    // 一个指针，用来追踪在字节向量中的位置
    const char *ptr = serializedData;

    // 从字节向量复制 epochNumber
    std::memcpy(&epochNumber, ptr, sizeof(epochNumber));
    ptr += sizeof(epochNumber);

    // 从字节向量复制 batchSize
    std::memcpy(&batchSize, ptr, sizeof(batchSize));
    ptr += sizeof(batchSize);

    // 从字节向量复制 workerNumber
    std::memcpy(&workerNumber, ptr, sizeof(workerNumber));
    ptr += sizeof(workerNumber);

    // 从字节向量复制 ipPortList 的大小和数据
    size_t ipPortListSize;
    std::memcpy(&ipPortListSize, ptr, sizeof(ipPortListSize));
    ptr += sizeof(ipPortListSize);
    ipPortList.assign(ptr, ptr + ipPortListSize);
}

void printHex(const std::vector<char> &data)
{
    for (unsigned char c : data)
    {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c) << " ";
    }
    std::cout << std::dec << std::endl; // 切回十进制模式，以免影响后续的输出
}

void writeIntToFile(const std::string &filename, int data)
{
    std::ofstream outfile(filename);
    if (outfile.is_open())
    {
        outfile << data;
        outfile.close();
        // std::cout << "数据已写入文件: " << filename << std::endl;
    }
    else
    {
        std::cerr << "无法打开文件: " << filename << std::endl;
    }
}