#include "KVRpcChannel.hpp"
#include "RpcHeader.pb.h"
#include "HostNet.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <Logger.hpp>
void KVRpcChannel::CallMethod(const MethodDescriptor *method, RpcController *controller, const Message *request, Message *response, Closure *done)
{
    const google::protobuf::ServiceDescriptor *sd = method->service(); // 获取该方法对应的服务描述器

    std::string service_name = sd->name(); // 获取服务名
    std::string method_name = method->name();

    // 获取请求参数的序列化字符串
    std::string request_str;
    if (request->SerializeToString(&request_str) == 0)
    {
        controller->SetFailed("serialize request arg error!");
        return;
    }
    uint32_t request_str_size = 0;
    request_str_size = request_str.size(); // 记录请求参数的实际长度，便于接收端根据长度解析

    // 填写rpc请求头信息
    rpc::RpcHeader rpcHeader;
    rpcHeader.set_service_name(service_name);
    rpcHeader.set_method_name(method_name);
    rpcHeader.set_args_len(request_str_size);

    // 获取rpc请求头序列化字符串
    std::string rpc_header_str;
    if (rpcHeader.SerializeToString(&rpc_header_str) == 0)
    {
        controller->SetFailed("serialize rpcheader error!");
        return;
    }
    uint32_t rpc_header_str_size = 0;
    rpc_header_str_size = rpc_header_str.size();

    // 组合rpc请求字符串
    std::string send_rpc_str;
    // 不能把字符串的数字存进去，比如 111存成"111",应该存储111的32位（4字节）数据，如果存字符串就没法固定头部信息了
    send_rpc_str += HostNet::host_to_net_uint32(rpc_header_str_size);
    send_rpc_str += rpc_header_str;
    send_rpc_str += request_str;

    // 打印调试信息
    std::cout << "====================================" << std::endl;
    std::cout << "header_size: " << rpc_header_str_size << std::endl;
    std::cout << "rpc_header_str:" << rpc_header_str << std::endl;
    std::cout << "service_name:" << service_name << std::endl;
    std::cout << "method_name:" << method_name << std::endl;
    std::cout << "args_str:" << request_str << std::endl;
    std::cout << "====================================" << std::endl;

    std::string responsestr;
    if (!TcpSendRecvMsg(send_rpc_str, responsestr))
    {
        controller->SetFailed(responsestr);
        return;
    }

    if (!response->ParseFromString(responsestr))
    {
        LOG_INFO("ip[%s],port[%d],response parse failed! file:%s line:%d", ip_myj.c_str(), port_myj, __FILE__, __LINE__);
        char buf[1024];
        sprintf(buf, "ip[%s],port[%d],response parse failed! file:%s line:%d", ip_myj.c_str(), port_myj, __FILE__, __LINE__);
        controller->SetFailed(buf);
        return;
    }
}

KVRpcChannel::KVRpcChannel(std::string ip, uint16_t port) : ip_myj(ip), port_myj(port), connected_myj(false),clientfd_myj(-1)
{
    Connection();
}

KVRpcChannel::~KVRpcChannel()
{
    connected_myj= false;
    if (clientfd_myj != -1)
    {
        close(clientfd_myj);
    }
}

bool KVRpcChannel::TcpSendRecvMsg(std::string &request, std::string &response)
{
    if (!connected_myj)
    {
        Connection();
        if(!connected_myj){
            char buf[1024];
            sprintf(buf, "ip[%s],port[%d],connect to peers failed! file:%s line:%d", ip_myj.c_str(), port_myj, __FILE__, __LINE__);
            response = std::string(buf);
            return false;
        }
    }
    if (send(clientfd_myj, request.c_str(), request.length(), 0) == -1)
    {
        LOG_INFO("ip[%s],port[%d],send failed! file:%s line:%d", ip_myj.c_str(), port_myj, __FILE__, __LINE__);
        char buf[1024];
        sprintf(buf, "ip[%s],port[%d],send failed! file:%s line:%d", ip_myj.c_str(), port_myj, __FILE__, __LINE__);
        response = std::string(buf);
        connected_myj = false;
        close(clientfd_myj);
        clientfd_myj=-1;
        return false;
    }
    int receive_size = 0;
    char buf[4096];
    if ((receive_size = recv(clientfd_myj, buf, sizeof(buf), 0)) == -1)
    {
        LOG_INFO("ip[%s],port[%d],receive msg failed! file:%s line:%d", ip_myj.c_str(), port_myj, __FILE__, __LINE__);
        char buf[1024];
        sprintf(buf, "ip[%s],port[%d],receive msg failed! file:%s line:%d", ip_myj.c_str(), port_myj, __FILE__, __LINE__);
        response = std::string(buf);
        connected_myj = false;
        close(clientfd_myj);   
        clientfd_myj=-1;
        return false;
    }
    response = std::string(buf, receive_size);
    return true;
}

void KVRpcChannel::Connection()
{
    if(!connected_myj)
    {
        clientfd_myj = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port_myj);
        inet_pton(AF_INET, ip_myj.c_str(), &server_addr.sin_addr);
        if (connect(clientfd_myj, (sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        {
            LOG_INFO("ip[%s],port[%d],connect failed!", ip_myj.c_str(), port_myj);
            std::this_thread::sleep_for(std::chrono::microseconds(1000));
            close(clientfd_myj);
            clientfd_myj = -1;
            return;
        }
        connected_myj = true;
    }
}
