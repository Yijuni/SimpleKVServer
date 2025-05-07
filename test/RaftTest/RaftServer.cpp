#include "KVRpcChannel.hpp"
#include "KVRpcProvider.hpp"
#include <string>
#include "Persister.hpp"
#include <memory>
#include <vector>
#include <ctype.h>
#include "LockQueue.hpp"
#include "Raft.hpp"
int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cout << "输入错误，格式：./server ip port" << std::endl;
        return 0;
    }
    std::string ip(argv[1]);
    std::uint16_t port = std::stoul(argv[2]);
    std::cout << "ip:" << ip << "port" << port << std::endl;
    std::shared_ptr<KVRaft> raft = std::make_shared<KVRaft>();
    //这里之所以这么写，首先为了确保raft服务器能正常初始化需要调用Make传入参数启动
    //但是Make参数需要和其他服务器连接的Stub，也就是已经连接上其他服务器了，
    //这就需要其他服务器已经成功启动了，每个服务器的代码都是一样的，要想连接上需要
    //其他服务器先Run起来，之后才能用Stub连接（阻塞连接）然后存入stub数组传给Make，所以连接操作和Make一定是在
    //Run之后的，不然谁都连不上谁，muduo网络库要求，EventLoop调用函数时一定是在它所在线程
    //调用的不然就会报错，所以需要单独开一个线程创建Provider(包含EventLoop)，然后执行Run，之后再用stub连接
    //其他服务器，然后传入Make真正的开始执行共识算法
    std::thread td([&]()
                   {    
        KVRpcProvider provider(ip, port);
        provider.NotifyService(raft.get());
        provider.Run(); });
    std::cout << "111";
    std::string name = ip + ":" + std::string(argv[2]);
    std::shared_ptr<Persister> persister = std::make_shared<Persister>("./" + name + "state", "./" + name + "snapshot");
    std::vector<std::shared_ptr<kvraft::KVRaftRPC_Stub>> stubs;
    std::shared_ptr<LockQueue<ApplyMsg>> applChan = std::make_shared<LockQueue<ApplyMsg>>();
    for (int i = 0; i < 3; i++)
    {
        if (8000 + i == port)
        {
            continue;
        }
        stubs.emplace_back(std::make_shared<kvraft::KVRaftRPC_Stub>(new KVRpcChannel("127.0.0.1", 8000 + i)));
    }
    raft->Make(stubs, name, persister, applChan);
    while (1)
    {
        ApplyMsg msg = applChan->pop();
    }
    // td.join();

    return 0;
}