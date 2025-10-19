#include "KVRpcChannel.hpp"
#include "KVRpcController.hpp"
#include "ShardCtrler.pb.h"
#include "ZKClient.hpp"
#include "ShardCtrlerClient.hpp"

int main()
{
    ShardCtrlerClient client("127.0.0.1", 2181,"client1");
    while (1)
    {
        std::cout<<"请输入要进行的操作"<<std::endl<<"join|leave|move|query"<<std::endl;
        std::string type;
        std::cin >> type;
        if (type == "join")
        {
            int groupnum;
            std::unordered_map<long long,std::vector<std::string>> newgroups;
            std::cout<<"请输入总共新加入的组数";
            std::cin>>groupnum;
            for(int n = 0;n<groupnum;n++){
                long long gid;
                int count;
                std::vector<std::string> servers;
                std::cout<<"请输入组id:";
                std::cin>>gid;
                std::cout<<"请输入该组服务器数量:";
                std::cin>>count;
                std::cout<<"请输入组内服务器名称（换行分隔）,名称应该是：ip:port形式"<<std::endl;
                for(int i=0;i<count;i++){
                    std::string name;
                    std::cin>>name;
                    servers.push_back(name);
                }
                newgroups[gid] = servers; 
            }
            if(client.Join(newgroups)){
                std::cout<<"Join请求成功"<<std::endl;
            }else{
                std::cout<<"Join请求失败"<<std::endl;
            }
        }
        else if (type == "leave")
        {
            int count;
            std::vector<long long> gids;
            std::cout<<"请输入要移除的组数目";
            std::cin>>count;
            for(int i=0;i<count;i++){
                long long gid;
                std::cin>>gid;
                gids.push_back(gid);
            }
            if(client.Leave(gids)){
                std::cout<<"leave请求成功"<<std::endl;
            }else{
                std::cout<<"leave请求失败"<<std::endl;
            }
        }
        else if(type=="move")
        {
            long long shard;
            long long gid;
            std::cout<<"请输入shardid:";
            std::cin>>shard;
            std::cout<<"请输入组id:";
            std::cin>>gid;
            if(client.Move(gid,shard)){
                std::cout<<"move请求成功"<<std::endl;
            }else{
                std::cout<<"move请求失败"<<std::endl;
            }
        }else{
            int num;
            shardctrler::Config config;
            std::cout<<"请输入配置号"<<std::endl;
            std::cin>>num;
            if(client.Query(num,config)){
                std::cout<<"query请求成功"<<std::endl;
                std::cout<<"实际配置号:"<<config.num()<<std::endl;
                for(int i=0;i<config.shards_size();i++){
                    printf("shard[%d],所属复制组[%ld]\n",i,config.shards(i));
                }
                std::cout<<"目前存在的复制组的信息"<<std::endl;
                for(auto &kv:config.groups()){
                    std::cout<<"组号:"<<kv.first<<std::endl;
                    for(int i=0;i<kv.second.serversname_size();i++){
                        std::cout<<kv.second.serversname(i)<<" ";
                    }
                }
            }else{
                std::cout<<"query请求失败"<<std::endl;
            }
        }
    }

    return 0;
}