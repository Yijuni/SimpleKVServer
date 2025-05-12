#include "ZKClient.hpp"
#include "Logger.hpp"
#include <chrono>
#include <memory>
#include <iostream>
ZKClient::ZKClient(std::string zkip, uint16_t zkport, int timeout)
    : zkip_myj(zkip), zkport_myj(zkport), zkConn_myj(nullptr), timeout_myj(timeout),isConnected_myj(false)
{
}

bool ZKClient::Connect()
{
    LOG_INFO("开始连接zookeeper服务器");
    std::string host = zkip_myj + ":" + std::to_string(zkport_myj);
    //zoo_init的context参数传入之后，可以用zoo_get_context在其他地方获取，也可以用watcher的watcherCtx访问到
    zkConn_myj = zookeeper_init(host.c_str(), globalWatcher, 10000, nullptr, this, 0);
    while (zkConn_myj == nullptr)
    {
        LOG_INFO("zookeeper连接失败尝试重连...");
        zkConn_myj = zookeeper_init(host.c_str(), globalWatcher, 10000, nullptr, this, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    while(!isConnected_myj){
        //等待连接完成
    }
    LOG_INFO("zookeeper服务器连接成功");
    return true;
}

void ZKClient::initChildWatcher(std::string path, watcherCallback callback)
{
    std::unique_lock<std::mutex> lock(callbackMutex_myj);
    callback_myj[path] = callback;
}

void ZKClient::registerChildWatcher(std::string path,std::vector<std::string> &info)
{
    LOG_INFO("给path[%s]事件注册回调", path.c_str());
    struct String_vector children;
    int ok = zoo_get_children(zkConn_myj, path.c_str(),1, &children);
    if(ok!=ZOK){
        LOG_ERROR("path[%s]孩子信息收取失败",path.c_str());
        return;
    }
    for(int i=0;i<children.count;i++){
        info.emplace_back(children.data[i]);
    }
    deallocate_String_vector(&children);
}

bool ZKClient::getChildInfo(std::string path, std::vector<std::string> &info)
{
    // zookeeper自带的用于返回孩子节点列表的结构体
    struct String_vector children;
    LOG_INFO("开始获取path[%s]的孩子信息", path.c_str());
    int ok = zoo_get_children(zkConn_myj, path.c_str(), 1, &children);
    if (ok != ZOK)
    {
        LOG_INFO("获取path[%s]子节点列表信息失败", path.c_str());
        return false;
    }
    for (int i = 0; i < children.count; i++)
    {
        info.emplace_back(children.data[i]);
    }
    deallocate_String_vector(&children);
    return true;
}

bool ZKClient::getPathData(std::string path, std::string &data)
{
    int buflen = 2048; // 初始化为0，用于获取实际需要的长度
    char buffer[buflen];
    LOG_INFO("获取path[%s]的信息", path.c_str());
    // 注意：：如果某条路径例如/kvserver/servers/server3的server3没有set任何值，buflen会返回-1
    int ok = zoo_get(zkConn_myj, path.c_str(), 0, buffer, &buflen, nullptr);
    if (ok != ZOK)
    {
        LOG_INFO("获取path[%s]信息失败,error:%d", path.c_str(), ok);
        return false;
    }
    if (buflen != -1)
    {
        data.assign(buffer, buflen);
    }

    return true;
}

void ZKClient::eventHandler(int type, int state, std::string path)
{

    if (type == ZOO_CHILD_EVENT)
    {
        watcherCallback call;
        {
            std::unique_lock<std::mutex> lock(callbackMutex_myj);
            auto iter = callback_myj.find(path);
            if (iter == callback_myj.end())
            {
                LOG_INFO("zookeeper客户端不包含path[%s]的回调函数", path.c_str());
                return;
            }
            call = iter->second;
        }
        // 执行回调
        call();
    }
}

void ZKClient::globalWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx)
{
    ZKClient* zk = static_cast<ZKClient*>(watcherCtx);
    if (type == ZOO_SESSION_EVENT)
    {
        if (state == ZOO_CONNECTED_STATE)
        {
            zk->isConnected_myj = true;
        }
    }else{
        zk->eventHandler(type,state,path);
    }
}
