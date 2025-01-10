#ifndef SERVER_DATA_HANDLER_H
#define SERVER_DATA_HANDLER_H

#include "CurrentController.h"
#include "common_data_handler.h"
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>
#define DATASET_PATH "/home/data/imagenet-32G/train"

std::vector<std::tuple<std::string, int, long>> collectImages(const std::string &datasetPath);
ImageData *loadAllImagesToVector(const std::string &datasetPath);
void processCategory(const std::filesystem::directory_entry &categoryEntry, ImageData *realImageData,
                     int imagePerDirectory);
void randomizeImageData(std::vector<int> &imageData);
std::vector<std::vector<std::pair<int, int>>> distributeBatches(int dataLength, int batchSize, int workerNumber);
std::vector<unsigned char> loadImageContent(const std::string &path);
std::vector<ImageData> loadImages(const std::vector<int> &imageData, const std::pair<int, int> &range,
                                  ImageData *allImageData);
std::string serializeMinibatch(const std::vector<ImageData> &images, int workerNumber, int epochNumber,
                               int batchNumber);
std::string createHeader(int workerNumber, int epochNumber, int batchNumber, int dataLength);
size_t sendMinibatch(std::vector<ImageData> loadedData, int sock, int batchNumber, int epochNumber, int workerNumber,
                     CurrentController *currentController);
std::vector<int> calculateEpochSizeForEachWorker(int dataLength, int batchSize, int workerNumber);

#endif // SERVER_DATA_HANDLER_H
