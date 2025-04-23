#ifndef KVSERVICE_HPP
#define KVSERVICE_HPP
/**
 * 2025-4-23 moyoj
 * KV服务器服务层的service，供客户端调用，完成了get put append 操作
 */
#include "KVService.pb.h"
class KVService:public kvservice::KVServiceRPC{
public:
    void Get(google::protobuf::RpcController* controller,const ::kvservice::GetRequest* request,
        ::kvservice::GetResponse* response,
        ::google::protobuf::Closure* done);
    void Put(google::protobuf::RpcController* controller,
        const ::kvservice::PutAppendRequest* request,
        ::kvservice::PutAppendResponse* response,
        ::google::protobuf::Closure* done);
    void Append(google::protobuf::RpcController* controller,
        const ::kvservice::PutAppendRequest* request,
        ::kvservice::PutAppendResponse* response,
        ::google::protobuf::Closure* done);
};

#endif