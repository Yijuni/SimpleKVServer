#include "AfterTimer.hpp"
#include <iostream>
#include <thread>
#include <chrono>
int main()
{
    AfterTimer timer(5, 1, []()
                     { std::cout << "成功执行回调" << std::endl; });
    std::cout << "开始执行" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    timer.Reset();
    std::cout << "非阻塞" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    timer.Reset();
    std::this_thread::sleep_for(std::chrono::seconds(10));
    timer.Reset();
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    std::cout << "程序退出" << std::endl;
    return 0;
}