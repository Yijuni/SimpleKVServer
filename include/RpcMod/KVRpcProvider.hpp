#ifndef KVRPCPROVIDER_HPP
#define KVRPCPROVIDER_HPP
/**
 * 2025-4-23 moyoj
 * 负责发布服务的类，想要发布服务需要写一个服务类继承proto文件生成的类，然后重写其中的
 * 方法，之后创建这个类的一个实例传递给该本文件下类的NotifyService函数，等请求到达之后
 * 回调函数会根据服务名和方法名调用对应服务类提供的方法。
 * 此类包含
 */
#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/TcpServer.h>
#include <unordered_map>
// 服务信息，保存了某个服务对象指针和对应的服务方法描述器
struct ServiceInfo
{
    // 保存服务对象指针
    google::protobuf::Service *m_service;
    // 保存服务方法名字及其对应描述器
    std::unordered_map<std::string, const google::protobuf::MethodDescriptor *> m_methodMap;
};

class KVRpcProvider
{
public:
    KVRpcProvider(std::string ip = "127.0.0.1", uint16_t port = 8000);
    // 发布服务
    void NotifyService(google::protobuf::Service *service);
    // 启动服务
    void Run();

private:
    // 主loop，监听连接
    muduo::net::EventLoop eventloop_myj;
    // 保存所有服务及其所有方法
    std::unordered_map<std::string, ServiceInfo> serviceinfo_myj;
    // ip port
    std::string ip_myj;
    uint16_t port_myj;
    // 服务器名称
    std::string name_myj;
    // 新连接或者断开连接的回调函数
    void OnConnection(const muduo::net::TcpConnectionPtr &);
    // 收到消息的回调函数
    void OnMessage(const muduo::net::TcpConnectionPtr &, muduo::net::Buffer *, muduo::Timestamp);
    // Closure回调，收到服务请求后调用相应服务，还需要返回结果，这个函数就是生成返回结果并回复
    void RPCSendResponse(const muduo::net::TcpConnectionPtr &, google::protobuf::Message *);
};

#endif