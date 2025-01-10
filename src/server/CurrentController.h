// CurrentController.h
#ifndef CURRENTCONTROLLER_H
#define CURRENTCONTROLLER_H

#include <condition_variable>
#include <cstdint> // For std::int64_t
#include <mutex>

class CurrentController
{
  public:
    CurrentController();

    bool tryDecreaseCapacity(long long amount);
    void increaseCapacity(long long amount);

  private:
    long long recieverCapacity;
    std::mutex recieverCapacityMutex;
    std::condition_variable cv;
};

#endif // CURRENTCONTROLLER_H
