#ifndef KVRPCCHANNEL_H
#define KVRPCCHANNEL_H
/**
 * 2025-4-22 moyoj
 * 重写rpcchannel的CallMethod函数，初始化rpc的stub时需要传入该类，
 * 这样才能调用其他服务器发布的服务，重写的CallMethod会由于多态被调用
 */
#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <atomic>
using namespace google::protobuf;

class KVRpcChannel : public google::protobuf::RpcChannel
{
public:
    KVRpcChannel(std::string ip="127.0.0.1", uint16_t port=8000);
    ~KVRpcChannel();
    void CallMethod(const MethodDescriptor *method, RpcController *controller, const Message *request, Message *response, Closure *done);
    bool TcpSendRecvMsg(std::string &request, std::string &response);
    void Connection();

private:
    uint16_t port_myj;
    std::string ip_myj;
    int clientfd_myj;
    std::atomic<bool> connected_myj;
};

#endif