// CurrentController.cpp
#include "CurrentController.h"

CurrentController::CurrentController() : recieverCapacity(50LL * 1000 * 1000 * 1000)
{
}

bool CurrentController::tryDecreaseCapacity(long long amount)
{
    std::unique_lock<std::mutex> lock(recieverCapacityMutex);
    while (recieverCapacity < amount)
    {
        cv.wait(lock);
    }
    recieverCapacity -= amount;
    return true;
}

void CurrentController::increaseCapacity(long long amount)
{
    {
        std::lock_guard<std::mutex> lock(recieverCapacityMutex);
        recieverCapacity += amount;
    }
    cv.notify_one();
}
