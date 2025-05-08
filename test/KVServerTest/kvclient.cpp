#include "KVRpcChannel.hpp"
#include "KVRpcController.hpp"
#include "KVService.pb.h"
#include "ZKClient.hpp"
enum ERRORID
{
    OK = 1,
    ErrNoKey = 2,
    ErrWrongLeader = 3,
    ErrTimeOut = 4,
};
int main()
{
    ZKClient zkcli("127.0.0.1", 2181, 10000);
    zkcli.Connect();
    int leaderid = 0;
    std::vector<std::string> info;
    std::vector<std::shared_ptr<kvservice::KVServiceRPC_Stub>> kvserver;
    zkcli.initChildWatcher("/kvserver/servers", []() {});
    zkcli.registerChildWatcher("/kvserver/servers", info);
    for (int i = 0; i < info.size(); i++)
    {
        std::string peerinfo;
        zkcli.getPathData("/kvserver/servers/" + info[i], peerinfo);
        int pos = peerinfo.find(":");
        std::string peerip = peerinfo.substr(0, pos);
        uint16_t port = std::stoi(peerinfo.substr(pos + 1));
        kvserver.emplace_back(std::make_shared<kvservice::KVServiceRPC_Stub>(new KVRpcChannel(peerip, port)));
    }
    std::cout << "全部成功连接" << std::endl;
    int requestid = 0;
    while (1)
    {
        string key;
        std::string value;
        std::string type;
        std::cin >> type;
        std::cin >> key;
        std::cin >> value;

        while (1)
        {
            if (type == "Get")
            {
                kvservice::GetRequest request;
                kvservice::GetResponse response;
                request.set_clientid("makabaka");
                request.set_requestid(requestid);
                request.set_key(key);
                KVRpcController controller;
                kvserver[leaderid]->Get(&controller, &request, &response, nullptr);

                if (controller.Failed())
                {
                    std::cout << controller.ErrorText()<<std::endl;
                    leaderid = (leaderid + 1) % kvserver.size();
                    continue;
                }
                if (response.resultcode().errorcode() != OK)
                {
                    leaderid = (leaderid + 1) % kvserver.size();
                    std::cout << "code:" << response.resultcode().errorcode() << " msg:" << response.resultcode().errormsg() << std::endl;
                    continue;
                }
                std::cout << "成功Get，leader:" << leaderid << std::endl;
                std::cout << "key:" << key << " value:" << response.value() << std::endl;
            }
            else
            {
                kvservice::PutAppendRequest request;
                kvservice::PutAppendResponse response;
                request.set_clientid("makabaka");
                request.set_requestid(requestid);
                request.set_key(key);
                request.set_value(value);
                KVRpcController controller;
                if (type == "Put")
                {
                    kvserver[leaderid]->Put(&controller, &request, &response, nullptr);
                }
                else
                {
                    kvserver[leaderid]->Append(&controller, &request, &response, nullptr);
                }
                if (controller.Failed())
                {
                    std::cout << controller.ErrorText()<<std::endl;
                    leaderid = (leaderid + 1) % kvserver.size();
                    continue;
                }
                if (response.resultcode().errorcode() != OK)
                {
                    leaderid = (leaderid + 1) % kvserver.size();
                    std::cout << "code:" << response.resultcode().errorcode() << " msg:" << response.resultcode().errormsg() << std::endl;
                    continue;
                }
                if (type == "Put")
                {
                    std::cout << "成功Put，leader:" << leaderid << std::endl;
                }
                else
                {
                    std::cout << "成功Append，leader:" << leaderid << std::endl;
                }
            }

            break;
        }
        requestid++;
    }

    return 0;
}