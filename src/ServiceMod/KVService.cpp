#include "KVService.hpp"

void KVService::Get(google::protobuf::RpcController *controller, const ::kvservice::GetRequest *request, ::kvservice::GetResponse *response, ::google::protobuf::Closure *done)
{
    std::cout<<"server>>receive"<<request->clientid()<<" get request"<<std::endl;
    response->mutable_resultcode()->set_errorcode(1);
    response->mutable_resultcode()->set_errormsg("ok");
    done->Run();
}

void KVService::Put(google::protobuf::RpcController *controller, const ::kvservice::PutAppendRequest *request, ::kvservice::PutAppendResponse *response, ::google::protobuf::Closure *done)
{
}

void KVService::Append(google::protobuf::RpcController *controller, const ::kvservice::PutAppendRequest *request, ::kvservice::PutAppendResponse *response, ::google::protobuf::Closure *done)
{
}
