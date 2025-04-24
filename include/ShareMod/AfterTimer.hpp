#ifndef AFTERTIMER_HPP
#define AFTERTIMER_HPP
/**
 * 2025-4-24 moyoj
 * 设计了一个定时器，可以传入回调函数，可以设置时间，可以重置时间
 * 时间到达后执行回调函数
 */
#include <functional>
#include <atomic>
#include <condition_variable>
using AfterTimerCallback = std::function<void()>;
class AfterTimer
{
public:
    // 设置时间 参数1：时间大小 参数2：事件类型（秒、毫秒) 0：毫秒 1：秒
    AfterTimer(double timelen=1000, int type=1, AfterTimerCallback callback=[](){});
    void SetTime(double timelen, int type);
    bool Reset();
    void SetCallback(AfterTimerCallback);

private:
    AfterTimerCallback callback_myj;
    std::atomic<double> timelen_myj;
    std::atomic<double> type_myj;
};
#endif