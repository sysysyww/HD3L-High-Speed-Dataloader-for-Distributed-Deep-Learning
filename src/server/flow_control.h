// flow_control.h

#ifndef FLOW_CONTROL_H
#define FLOW_CONTROL_H

#include "CurrentController.h" // 包含CurrentController类定义
#include <vector>

// 函数声明
void handleCapacityIncreaseRequest(int clientSocket, std::vector<std::unique_ptr<CurrentController>> &controllers);
void listenForCapacityUpdates(std::vector<std::unique_ptr<CurrentController>> &controllers, int listenSocket);

#endif // FLOW_CONTROL_H
