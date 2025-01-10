#ifndef UTILS_H
#define UTILS_H
constexpr int PORT = 8080;

#include <string>
#include <vector>
// 例子函数声明
std::vector<char> serializeData(int epochNumber, int batchSize, int workerNumber, const std::string &ipPortList);
void deserializeData(const char *serializedData, int &epochNumber, int &batchSize, int &workerNumber,
                     std::string &ipPortList);
std::vector<std::pair<std::string, uint16_t>> parseIpPort(const std::string &input);
void printHex(const std::vector<char> &data);
void writeIntToFile(const std::string &filename, int data);
#endif // UTILS_H