#include "CurrentController.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <iostream>
#include <netinet/in.h> // 对于地址结构
#include <sys/socket.h> // 对于socket函数
#include <thread>
#include <unistd.h> // 对于UNIX系统的read和close函数
#include <vector>

void handleCapacityIncreaseRequest(int clientSocket, std::vector<std::unique_ptr<CurrentController>> &controllers)
{
    int message[2]; // 用于接收rank和additionalCapacity
    ssize_t bytesRead = read(clientSocket, message, sizeof(message));

    if (bytesRead < 0)
    {
        std::cerr << "Error reading from socket." << std::endl;
        close(clientSocket);
        return;
    }

    int rank = message[0];
    int additionalCapacity = message[1];

    controllers[rank]->increaseCapacity(additionalCapacity);

    close(clientSocket);
}

void listenForCapacityUpdates(std::vector<std::unique_ptr<CurrentController>> &controllers, int listenSocket)
{
    boost::asio::thread_pool localThreadPool(2);
    while (true)
    {
        int clientSocket = accept(listenSocket, nullptr, nullptr);
        if (clientSocket < 0)
        {
            std::cerr << "Error accepting client connection." << std::endl;
            continue;
        }

        boost::asio::post(localThreadPool,
                          [clientSocket, &controllers]() { handleCapacityIncreaseRequest(clientSocket, controllers); });
    }
}