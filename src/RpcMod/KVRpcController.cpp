#include "KVRpcController.hpp"

KVRpcController::KVRpcController()
{
    failed_myj = false;
    errmsg_myj = "";
}

void KVRpcController::Reset()
{
    failed_myj = false;
    errmsg_myj = "";
}

bool KVRpcController::Failed() const
{
    return failed_myj;
}

std::string KVRpcController::ErrorText() const
{
    return errmsg_myj;
}

void KVRpcController::SetFailed(const std::string &reason)
{
    failed_myj = true;
    errmsg_myj = reason;
}

void KVRpcController::StartCancel()
{
}

bool KVRpcController::IsCanceled() const
{
    return false;
}

void KVRpcController::NotifyOnCancel(google::protobuf::Closure *callback)
{
}
