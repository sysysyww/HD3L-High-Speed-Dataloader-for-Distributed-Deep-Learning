#ifndef CLIENT_DATA_HANDLER_H
#define CLIENT_DATA_HANDLER_H

#include "common_data_handler.h"
#include <string>
#include <vector>

void parseHeader(const std::string &header, int &workerNumber, int &epochNumber, int &batchNumber, int &dataLength);
std::vector<ImageData> deserialize_batch(const std::vector<char> &serialized_batch);

#endif // CLIENT_DATA_HANDLER_H