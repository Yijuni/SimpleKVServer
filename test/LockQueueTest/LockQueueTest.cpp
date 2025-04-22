#include <thread>
#include <iostream>
#include "LockQueue.hpp"
#include <random>
int main() {
    LockQueue<int> chan(0); // 无缓冲模式

    std::thread sender([&] {
        while(1){
            std::cout << "trying to push..." << std::endl;
            chan.push(random()%1000);
            std::cout << "data sent" << std::endl;
        }

    });

    std::thread receiver([&] {
        while(1){
            std::this_thread::sleep_for(std::chrono::seconds(5)); 
            std::cout << "trying to pop..." << std::endl;
            int val = chan.pop();
            std::cout << "got " << val << std::endl;
        }
    });

    sender.join();
    receiver.join();
    return 0;
}