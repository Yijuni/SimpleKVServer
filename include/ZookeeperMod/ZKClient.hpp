#ifndef ZKCLIENT_HPP
#define ZKCLIENT_HPP
#define THREADED
/**
 * 2025-4-28 moyoj
 * 用来初连接zookeeper服务器并且监视节点是否发生变化
 */

#include <string>
#include <zookeeper/zookeeper.h>
#include <functional>
#include <vector>
#include <string>
#include <semaphore.h>
#include <unordered_map>
#include <mutex>
#include <atomic>
class ZKClient{
public:
    using watcherCallback = std::function<void()>; 
    ZKClient(std::string zkip="127.0.0.1",uint16_t zkport=8010,int timeout=10000);
    bool Connect();
    //注册该路径下节点发生变化的回调函数
    void initChildWatcher(std::string path,watcherCallback);
    //注册监视某路径下节点的变化
    void registerChildWatcher(std::string path,std::vector<std::string> &info);
    //获取某路径下孩子节点信息
    bool getChildInfo(std::string path,std::vector<std::string>& info);
    //获取某路径下存储的信息
    bool getPathData(std::string path,std::string& data);
    //事件处理函数,主要处理节点状态的
    void eventHandler(int type,int state,std::string path);
    //全局watcher函数,主要用来检测连接状态的
    static void globalWatcher(zhandle_t *zh, int type,int state, const char *path,void *watcherCtx);
private:

    std::string zkip_myj;
    uint16_t zkport_myj;
    zhandle_t *zkConn_myj;
    int timeout_myj;
    std::mutex callbackMutex_myj;
    std::unordered_map<std::string,watcherCallback> callback_myj;
    std::atomic<bool> isConnected_myj; 
};
#endif