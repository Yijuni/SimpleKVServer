#ifndef KVRPCCONTROLLER_H
#define KVRPCCONTROLLER_H
/*
2025-4-22 moyoj
重写RpcController，告知调用rpc的程序调用是否发生错误，如果发生错误错误信息是什么
*/
#include <google/protobuf/service.h>

class KVRpcController : public google::protobuf::RpcController
{
public:
    KVRpcController();
    void Reset();                              // 重置状态
    bool Failed() const;                       // 是否发生错误
    std::string ErrorText() const;             // 错误信息
    void SetFailed(const std::string &reason); // 设置错误信息

    // 下面的其实没用到
    void StartCancel();
    bool IsCanceled() const;
    void NotifyOnCancel(google::protobuf::Closure *callback);

private:
    bool failed_myj;        // rpc方法执行过程中的状态
    std::string errmsg_myj; // RPC方法执行过程中的错误信息
};

#endif