#include "AfterTimer.hpp"
#include <thread>
#include <chrono>
#include <iostream> 
#include <random>
AfterTimer::AfterTimer(double timelen, int type, AfterTimerCallback callback)
    : timelen_myj(timelen), type_myj(type), callback_myj(callback),
      running_myj(true), timecount_myj(0), isReset(false)
{
    std::thread td(std::bind(&AfterTimer::run, this));
    td.detach();
}

void AfterTimer::SetTime(double timelen, int type)
{
    timelen_myj = timelen;
    type_myj = type;
    timecount_myj = 0;
}

bool AfterTimer::Reset()
{
    timecount_myj = 0;
    isReset = true;
    return true;
}

bool AfterTimer::RandomReset(double begin, double end)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(begin,end);
    timelen_myj = dist(gen);
    timecount_myj = 0;
    isReset = true;
    return true;
}

void AfterTimer::SetCallback(AfterTimerCallback callback)
{
    callback_myj = callback;
}

void AfterTimer::run()
{
    while (1)
    {
        if (!running_myj)
        {
            break;
        }

        if (!isReset)
        {
            continue;
        }else{
            isReset = false;
        }
        while (timecount_myj < timelen_myj)
        {
            if (type_myj)
            { // 秒
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            else
            { // 毫秒
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
            timecount_myj = timecount_myj + 1;
        }
        callback_myj();
    }
}

AfterTimer::~AfterTimer()
{
    running_myj = false;
}
