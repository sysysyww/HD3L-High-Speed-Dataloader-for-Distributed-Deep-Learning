#ifndef COMMON_DATA_HANDLER_H
#define COMMON_DATA_HANDLER_H
#include <cstring>
#include <iostream>
#include <istream>
#include <ostream>
#include <vector>

struct ImageData
{
    int label;
    long globalIndex;
    std::vector<unsigned char> content;

    // 自定义序列化
    void serialize(std::ostream &out) const
    {
        // 写入label和globalIndex
        out.write(reinterpret_cast<const char *>(&label), sizeof(label));
        out.write(reinterpret_cast<const char *>(&globalIndex), sizeof(globalIndex));
        // 写入content大小，然后是content数据
        size_t size = content.size();
        out.write(reinterpret_cast<const char *>(&size), sizeof(size));
        if (size > 0)
        {
            out.write(reinterpret_cast<const char *>(content.data()), size);
        }
    }

    const char *deserialize(const char *dataPtr, size_t dataSize)
    {
        if (dataSize < sizeof(label) + sizeof(globalIndex) + sizeof(size_t))
        {
            std::cerr << "Data size too small to deserialize ImageData." << std::endl;
            return nullptr; // 数据不足以反序列化基本字段
        }

        // 反序列化label
        memcpy(&label, dataPtr, sizeof(label));
        dataPtr += sizeof(label);
        dataSize -= sizeof(label);

        // 反序列化globalIndex
        memcpy(&globalIndex, dataPtr, sizeof(globalIndex));
        dataPtr += sizeof(globalIndex);
        dataSize -= sizeof(globalIndex);

        // 反序列化content的大小
        size_t size;
        memcpy(&size, dataPtr, sizeof(size));
        dataPtr += sizeof(size_t);
        dataSize -= sizeof(size_t);

        // 检查content的大小是否合法
        if (size > dataSize)
        {
            std::cerr << "Content size larger than remaining data." << std::endl;
            return nullptr;
        }

        // 反序列化content
        content.resize(size);
        memcpy(content.data(), dataPtr, size);
        dataPtr += size;

        return dataPtr; // 返回新的指针位置
    }
};

#endif // COMMON_DATA_HANDLER_H