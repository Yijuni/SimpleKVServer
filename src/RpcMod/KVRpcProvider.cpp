#include "KVRpcProvider.hpp"
#include "Logger.hpp"
#include "HostNet.hpp"
#include "RpcHeader.pb.h"
KVRpcProvider::KVRpcProvider(std::string ip, uint16_t port) : ip_myj(ip), port_myj(port), name_myj("")
{
}
void KVRpcProvider::NotifyService(google::protobuf::Service *service)
{
    ServiceInfo newservice;
    newservice.m_service = service;

    // 获取服务对象描述器
    const google::protobuf::ServiceDescriptor *servicedesc = service->GetDescriptor();
    // 获取服务名字
    std::string servicename = servicedesc->name();

    // 得到服务对象方法数目
    int methodcount = servicedesc->method_count();

    LOG_INFO("server>>ip[%s]:port[%d] start,service name :%s file:%s line:%d", ip_myj.c_str(), port_myj, servicename.c_str(), __FILE__, __LINE__);

    for (int i = 0; i < methodcount; i++)
    {
        // 获得方法描述器
        const google::protobuf::MethodDescriptor *methoddesc = servicedesc->method(i);
        std::string methodname = methoddesc->name();
        newservice.m_methodMap.insert({methodname, methoddesc});
        LOG_INFO("server>>ip[%s]:port[%d] method name :%s file:%s line:%d", ip_myj.c_str(), port_myj, methodname.c_str(), __FILE__, __LINE__);
    }

    // 将这个服务插入服务表
    serviceinfo_myj.insert({servicename, newservice});
}

void KVRpcProvider::Run()
{
    muduo::net::InetAddress address(ip_myj, port_myj);
    // 创建server
    muduo::net::TcpServer server(&eventloop_myj, address, name_myj);
    // 绑定回调，等有新连接会分配回调给链接
    server.setConnectionCallback(std::bind(&KVRpcProvider::OnConnection, this, std::placeholders::_1));
    server.setMessageCallback(std::bind(&KVRpcProvider::OnMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    // 设置线程数 IO线程+work线程
    server.setThreadNum(4);

    LOG_INFO("server>>ip[%s]:port[%d] name :%s start, file:%s line:%d", ip_myj.c_str(), port_myj, name_myj.c_str(), __FILE__, __LINE__);

    server.start();
    eventloop_myj.loop();
}

void KVRpcProvider::OnConnection(const muduo::net::TcpConnectionPtr &connptr)
{
    if (!connptr->connected())
    {
        connptr->shutdown();
    }
}

void KVRpcProvider::OnMessage(const muduo::net::TcpConnectionPtr &connptr, muduo::net::Buffer *recvbuf, muduo::Timestamp t)
{
    // 接收到的信息转为string
    std::string recvstr = recvbuf->retrieveAllAsString();

    // 读取前四个字节获取请求头长度，这是规定的
    uint32_t headersize = 0;
    headersize = HostNet::net_to_host_uint32(recvstr.substr(0, 4));

    // 获取请求头信息
    std::string headerstr = recvstr.substr(4, headersize);

    // 反序列化请求头
    rpc::RpcHeader header;
    if (!header.ParseFromString(headerstr))
    {
        LOG_INFO("server>>ip[%s]:port[%d] request header parse failed! file:%s line:%d", ip_myj.c_str(), port_myj, __FILE__, __LINE__);
        connptr->shutdown();
        return;
    }

    std::string servicename = header.service_name();
    std::string methodname = header.method_name();
    uint32_t argsize = header.args_len();

    // 获取请求参数信息
    std::string requeststr = recvstr.substr(4 + headersize, argsize);

    // 打印调试信息
    std::cout << "====================================" << std::endl;
    std::cout << "header_size: " << headersize << std::endl;
    std::cout << "rpc_header_str:" << headerstr << std::endl;
    std::cout << "service_name:" << servicename << std::endl;
    std::cout << "method_name:" << methodname << std::endl;
    std::cout << "args_str:" << requeststr << std::endl;
    std::cout << "====================================" << std::endl;

    // 根据服务名获取服务迭代器
    auto service_iter = serviceinfo_myj.find(servicename);
    if (service_iter == serviceinfo_myj.end())
    {
        LOG_INFO("server>>ip[%s]:port[%d] request service not exist! file:%s line:%d", ip_myj.c_str(), port_myj, __FILE__, __LINE__);
        connptr->shutdown();
        return;
    }

    // 根据方法名获取方法迭代器
    auto method_iter = service_iter->second.m_methodMap.find(methodname);
    if (service_iter == serviceinfo_myj.end())
    {
        LOG_INFO("server>>ip[%s]:port[%d] request method not exist! file:%s line:%d", ip_myj.c_str(), port_myj, __FILE__, __LINE__);
        connptr->shutdown();
        return;
    }

    // 获取service对象
    google::protobuf::Service *service = service_iter->second.m_service;
    // 获取method对象
    const google::protobuf::MethodDescriptor *methoddesc = method_iter->second;

    // 获取方法请求参数类型，创建一个实例并反序列化出请求参数
    google::protobuf::Message *request = service->GetRequestPrototype(methoddesc).New();
    if (!request->ParseFromString(requeststr))
    {
        LOG_INFO("server>>ip[%s]:port[%d] request parse failed! file:%s line:%d", ip_myj.c_str(), port_myj, __FILE__, __LINE__);
        connptr->shutdown();
        return;
    }
    // 获取方法回复参数类型，并创建一个实例
    google::protobuf::Message *response = service->GetResponsePrototype(methoddesc).New();

    // 创建一个closure回调，等服务执行完需要执行该回调来执行发送response到客户端
    google::protobuf::Closure *closure =
        google::protobuf::NewCallback<KVRpcProvider, const muduo::net::TcpConnectionPtr &,
                                      google::protobuf::Message *>(this, &KVRpcProvider::RPCSendResponse, connptr, response);

    // 调用方法
    service->CallMethod(methoddesc, nullptr, request, response, closure);
}

void KVRpcProvider::RPCSendResponse(const muduo::net::TcpConnectionPtr &connptr, google::protobuf::Message *response)
{
    std::string responsestr;
    // 序列化回复信息
    if (!response->SerializeToString(&responsestr))
    {
        LOG_INFO("server>>ip[%s]:port[%d] response serialize failed! file:%s line:%d", ip_myj.c_str(), port_myj, __FILE__, __LINE__);
        connptr->shutdown();
        return;
    }
    // 发送回复消息
    connptr->send(responsestr);
}
