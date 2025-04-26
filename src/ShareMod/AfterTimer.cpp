#include "AfterTimer.hpp"
#include <thread>
#include <chrono>
#include <iostream>
AfterTimer::AfterTimer(double timelen, int type, AfterTimerCallback callback)
    : timelen_myj(timelen), type_myj(type), callback_myj(callback)
{
}

void AfterTimer::SetTime(double timelen, int type)
{
    timelen_myj = timelen;
    type_myj = type;
}

bool AfterTimer::Reset()
{
    std::thread td([&]()
                   {
        if (type_myj)
        { // 秒
            std::this_thread::sleep_for(std::chrono::seconds(timelen_myj));
        }
        else
        { // 毫秒
            std::this_thread::sleep_for(std::chrono::microseconds(timelen_myj));
        }
        callback_myj(); 
    });

    td.detach();
    return true;
}

void AfterTimer::SetCallback(AfterTimerCallback callback)
{
    callback_myj = callback;
}
