#ifndef LOCKQUEUE_HPP
#define LOCKQUEUE_HPP
/**
 * 2025-4-22 moyoj
 * 线程安全的队列主要是为了代替go语言的channel，可以自定义容量，创建时传入参数即可
 */
#include <queue>
#include <thread>
#include <condition_variable>
#include <mutex>

template <typename T>
class LockQueue
{
public:
    LockQueue(int capacity = 0);
    void push(const T &data);
    T pop();
    void setCapacity(int capacity);
private:
    std::queue<T> queue_myj;
    std::mutex mutex_myj;
    T pending_value_myj;           // 无缓冲临时存储
    bool has_value_myj = false;    // 无缓冲数据就绪标志
    bool has_receiver_myj = false; // 无缓冲接收者就绪标志
    std::condition_variable condpop_myj;
    std::condition_variable condpush_myj;
    int capacity_myj;
};

template <typename T>
inline LockQueue<T>::LockQueue(int capacity) : capacity_myj(capacity)
{
}
template <typename T>
inline void LockQueue<T>::push(const T& data) {
    std::unique_lock lock(mutex_myj);
    if (capacity_myj == 0) {            // 无缓冲模式：等待接收者就绪
        condpush_myj.wait(lock, [this] { return has_receiver_myj; });
        pending_value_myj = data;       // 直接传递数据给接收者
        has_value_myj = true;
        has_receiver_myj = false;
        condpop_myj.notify_one();       // 通知接收者取数据
    } else {
        condpush_myj.wait(lock, [this] { return queue_myj.size() < capacity_myj; });
        queue_myj.push(data);
        condpop_myj.notify_one();
    }
}

template <typename T>
inline T LockQueue<T>::pop() {
    std::unique_lock lock(mutex_myj);
    if (capacity_myj == 0) {
       
        has_receiver_myj = true;                                    // 无缓冲模式：通知发送者可以发送数据
        condpush_myj.notify_one();                                  // 唤醒等待的发送者
        condpop_myj.wait(lock, [this] { return has_value_myj; });   // 等待有数据
        T data = pending_value_myj;
        has_value_myj = false;
        return data;
    } else {
        condpop_myj.wait(lock, [this] { return !queue_myj.empty(); });
        T data = queue_myj.front();
        queue_myj.pop();
        condpush_myj.notify_one();
        return data;
    }
}

template <typename T>
inline void LockQueue<T>::setCapacity(int capacity)
{
    capacity_myj = capacity;
}

#endif