#ifndef SHAREDDATA_H
#define SHAREDDATA_H

#include <atomic>
#include "Position.h"

// Shared state used by the top and bottom solver threads to communicate.
struct SharedData
{
    std::atomic<Position> stop;

    std::atomic_bool isCollision;   
    char pad3[3];                   

    char pad4[4];                   

    SharedData()
        : stop(Position(-1, -1)),
        isCollision(false),
        pad3{ 0,0,0 },
        pad4{ 0,0,0,0 }
    {
    }
};


#endif // SHAREDDATA_H
