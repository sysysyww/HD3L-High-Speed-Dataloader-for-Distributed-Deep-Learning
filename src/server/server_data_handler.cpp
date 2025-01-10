#include "server_data_handler.h"
#include <algorithm>
#include <arpa/inet.h> // 提供inet_addr()函数
#include <chrono>
#include <cmath>   // 对于std::ceil
#include <cstdint> // For fixed width integers
#include <cstring> // 如果你使用了memset等字符串/内存操作函数
#include <execution>
#include <fstream>
#include <iostream>
#include <iterator>
#include <netinet/in.h> // 提供sockaddr_in结构体和htons()函数
#include <random>
#include <sstream>
#include <string>
#include <sys/socket.h> // 提供socket(), connect(), send()等函数
#include <sys/uio.h>
#include <tuple>
#include <utility> // 对于std::pair
#include <vector>

namespace fs = std::filesystem;

std::vector<std::tuple<std::string, int, long>> collectImages(const std::string &datasetPath)
{
    std::vector<std::tuple<std::string, int, long>> imageData;
    long globalIndex = 0; // 全局序号

    // 遍历datasetPath下的每个子目录，每个子目录代表一个类别
    for (const auto &categoryEntry : fs::directory_iterator(datasetPath))
    {
        if (categoryEntry.is_directory())
        {
            int label = std::stoi(categoryEntry.path().filename().string()); // 从文件夹名称获取标签

            // 遍历类别目录下的每个文件
            for (const auto &entry : fs::directory_iterator(categoryEntry.path()))
            {
                if (entry.is_regular_file())
                {
                    // 为每个文件收集路径、标签和全局序号
                    imageData.emplace_back(entry.path().string(), label, globalIndex++);
                }
            }
        }
    }

    return imageData;
}

void processCategory(const fs::directory_entry &categoryEntry, ImageData *realImageData, int imagePerDirectory)
{
    if (categoryEntry.is_directory())
    {
        int label = std::stoi(categoryEntry.path().filename().string());

        int i = 0;
        for (const auto &entry : fs::directory_iterator(categoryEntry.path()))
        {
            if (entry.is_regular_file())
            {
                int index = label * imagePerDirectory + i;
                std::vector<unsigned char> content = loadImageContent(entry.path().string());
                realImageData[index].label = label;
                realImageData[index].globalIndex = label;
                realImageData[index].content = std::move(content);
                i++;
            }
        }
        std::cout << "Finish " << label << std::endl; // 注意：在并行执行中，cout可能不是线程安全的
    }
}

ImageData *loadAllImagesToVector(const std::string &datasetPath)
{
    // 提前排序，只在测试中使用！！！！
    std::vector<fs::directory_entry> categories;
    for (const auto &entry : fs::directory_iterator(datasetPath))
    {
        if (entry.is_directory())
        {
            categories.push_back(entry);
        }
    }
    std::sort(categories.begin(), categories.end(), [](const fs::directory_entry &a, const fs::directory_entry &b) {
        return a.path().filename().string() < b.path().filename().string();
    });

    int imagePerDirectory = 300;
    int directoryNumber = 1000;
    int arraySize = imagePerDirectory * directoryNumber;
    ImageData *realImageData = new ImageData[arraySize];

    std::for_each(std::execution::par, categories.begin(), categories.end(),
                  [&](const fs::directory_entry &entry) { processCategory(entry, realImageData, imagePerDirectory); });

    return realImageData;
}

void randomizeImageData(std::vector<int> &imageData)
{
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::shuffle(imageData.begin(), imageData.end(), std::default_random_engine(seed));
}

// 三维向量，最外层长度是worker数量，每个元素也是个vector，代表了该worker在本次epoch中分到的minibaches的边界pairs
// 这里逻辑在3.25进行了修改，batchsize变成了每个节点处理的batchsize，原先batchsize是所有节点一起分享的size，逻辑可以简化，
// 但暂时直接在调用时用batchsize*workernumber替换原先的batchsize，可以得到相同的结果
std::vector<std::vector<std::pair<int, int>>> distributeBatches(int dataLength, int batchSize, int workerNumber)
{
    // 计算总共需要多少个batch
    int totalBatches = std::ceil(dataLength / static_cast<float>(batchSize));

    // 初始化返回的结构
    std::vector<std::vector<std::pair<int, int>>> batchesMedaData(workerNumber);

    for (int batchIndex = 0; batchIndex < totalBatches; ++batchIndex)
    {
        int currentBatchSize = std::min(dataLength - batchIndex * batchSize, batchSize);
        // skip last not compelete batch
        if (currentBatchSize != batchSize)
        {
            break;
        }
        int dataPerWorker = currentBatchSize / workerNumber;
        int extraData = currentBatchSize % workerNumber;

        int dataStart = batchIndex * batchSize;
        for (int workerIndex = 0; workerIndex < workerNumber; ++workerIndex)
        {
            // 计算这个worker在这个batch中的数据起始位置
            int start = dataStart + workerIndex * dataPerWorker;

            // 计算结束位置，注意最后一个worker可能会拿到较多的数据
            int end = start + dataPerWorker + ((workerIndex + 1) == workerNumber ? extraData : 0);

            // 将这个batch的数据范围分配给相应的worker
            if (start < dataLength)
            { // 确保不会超出数据长度
                batchesMedaData[workerIndex].emplace_back(start, std::min(end, dataLength));
            }
        }
    }

    return batchesMedaData;
}

// not using anymore
std::vector<int> calculateEpochSizeForEachWorker(int dataLength, int batchSize, int workerNumber)
{
    int totalBatches = std::ceil(dataLength / static_cast<float>(batchSize));
    std::vector<int> epochSizes(workerNumber);

    for (int batchIndex = 0; batchIndex < totalBatches; ++batchIndex)
    {
        int currentBatchSize = std::min(dataLength - batchIndex * batchSize, batchSize);
        // skip last not compelete batch
        if (currentBatchSize != batchSize)
        {
            break;
        }
        int dataPerWorker = currentBatchSize / workerNumber;
        int extraData = currentBatchSize % workerNumber;

        int dataStart = batchIndex * batchSize;
        for (int workerIndex = 0; workerIndex < workerNumber; ++workerIndex)
        {
            // 计算这个worker在这个batch中的数据起始位置
            int start = dataStart + workerIndex * dataPerWorker;

            // 计算结束位置，注意最后一个worker可能会拿到较多的数据
            int end = start + dataPerWorker + ((workerIndex + 1) == workerNumber ? extraData : 0);

            // 将这个batch的数据范围分配给相应的worker
            if (start < dataLength)
            { // 确保不会超出数据长度
                epochSizes[workerIndex] += end - start;
            }
        }
    }
    return epochSizes;
}

std::vector<unsigned char> loadImageContent(const std::string &path)
{
    // 打开文件
    std::ifstream file(path, std::ios::binary);

    // 检查文件是否成功打开
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file: " + path);
    }

    // 读取文件内容到 vector 中
    std::vector<unsigned char> content((std::istreambuf_iterator<char>(file)), (std::istreambuf_iterator<char>()));

    // 关闭文件
    file.close();

    return content;
}

std::vector<ImageData> loadImages(const std::vector<int> &imageData, const std::pair<int, int> &range,
                                  ImageData *allImageData)
{
    std::vector<ImageData> loadedImages;

    // 确保范围有效
    int start = range.first;
    int end = range.second;
    if (range.second > static_cast<int>(imageData.size()))
    {
        throw std::runtime_error("range error");
    }

    for (int i = start; i < end; ++i)
    {
        // 获得随机化后列表的第i个元组的long索引
        int index = imageData[i];

        //  复制ImageData实例并添加到结果列表中
        loadedImages.push_back(allImageData[index]);
    }
    size_t bytes = loadedImages.size() * sizeof(loadedImages[0]);
    //     std::cout << "image size before serialize = " << bytes << std::endl;
    //     std::cout << "size of imagedata = " << sizeof(loadedImages[0]) << std::endl;
    return loadedImages;
}

std::string serializeMinibatch(const std::vector<ImageData> &images, int workerNumber, int epochNumber, int batchNumber)
{

    std::stringstream ss;
    // 序列化ImageData向量到ss
    for (const auto &image : images)
    {
        image.serialize(ss);
    }

    std::string serializedData = ss.str();
    // std::cout << "image size after serialize = " << serializedData.size() << std::endl;
    std::string header = createHeader(workerNumber, epochNumber, batchNumber, serializedData.length());

    // use iovec to avoid long copy
    std::string finalData = header + serializedData;
    return finalData;
}

std::string createHeader(int workerNumber, int epochNumber, int batchNumber, int dataLength)
{
    std::string header;

    header.push_back(static_cast<char>(workerNumber & 0xFF));
    header.push_back(static_cast<char>(epochNumber & 0xFF));
    header.push_back(static_cast<char>((batchNumber >> 8) & 0xFF)); // Higher 8 bits of 16
    header.push_back(static_cast<char>(batchNumber & 0xFF));        // Lower 8 bits of 16
    header.push_back(static_cast<char>((dataLength >> 24) & 0xFF)); // Highest 8 bits of 32
    header.push_back(static_cast<char>((dataLength >> 16) & 0xFF));
    header.push_back(static_cast<char>((dataLength >> 8) & 0xFF));
    header.push_back(static_cast<char>(dataLength & 0xFF)); // Lowest 8 bits of 32

    return header;
}

size_t sendMinibatch(std::vector<ImageData> loadedData, int sock, int batchNumber, int epochNumber, int workerNumber,
                     CurrentController *currentController)
{
    std::string miniBatchWithHeader = serializeMinibatch(loadedData, workerNumber, epochNumber, batchNumber);

    ssize_t result;
    // large batchsize and large image could cause problem
    size_t total = 0;
    const char *dataPtr = miniBatchWithHeader.data();
    size_t dataLeft = miniBatchWithHeader.size();

    if (currentController->tryDecreaseCapacity(miniBatchWithHeader.size()))
    {
        while (total < dataLeft)
        {
            result = send(sock, dataPtr + total, dataLeft - total, 0);
            if (result < 0)
            {
                printf("send < 0\n");
                return 0;
            }
            total += result;
        }
    }
    return total;
}

// int main()
// {
//     std::vector<std::tuple<std::string, int, long>> imageData = collectImages(DATASET_PATH);
//     randomizeImageData(imageData);
//     std::vector<ImageData> loadedData = loadImages(imageData, std::pair<int, int>(5, 10));
//     std::string serializedData = serializeMinibatch(loadedData, 0, 0, 1);
// }

// int main()
// {
//     int sock = socket(AF_INET, SOCK_STREAM, 0);

//     // initiate address
//     struct sockaddr_in serv_addr;
//     serv_addr.sin_family = AF_INET;
//     serv_addr.sin_port = htons(8080);
//     serv_addr.sin_addr.s_addr = inet_addr("8.130.174.110");

//     // connect to server
//     if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
//     {
//         perror("Connection failed");
//         return -1;
//     }

//     std::vector<std::tuple<std::string, int, long>> imageData = collectImages(DATASET_PATH);
//     randomizeImageData(imageData);

//     std::vector<ImageData> loadedData = loadImages(imageData, std::pair<int, int>(5, 60));

//     if (sendMinibatch(loadedData, sock, 0, 0, 0) < 0)
//     {
//         printf("send faile\n");
//     }
//     return 0;
// }