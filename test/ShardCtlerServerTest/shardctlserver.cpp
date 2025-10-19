#include "KVServer.hpp"
#include <string>
int main(int argc,char** argv){
    if(argc<6){
        std::cout<<"正确格式: ./shardctlserver ip port zkip zkport servertype(kvservice|shardctler)";
        return 0;
    }
    std::string sip(argv[1]);
    uint16_t sport = std::atoi(argv[2]);
    std::string zkip(argv[3]);
    uint16_t zkport = std::atoi(argv[4]);
    std::string servertype(argv[5]);
    SERVICE_TYPE type;
    if(servertype=="kvservice"){
        type = SERVICE_TYPE::KVSERVICE;
    }else if(servertype=="shardctler"){
        type = SERVICE_TYPE::SHARDCTRLER;
    }else
    {
        type = SERVICE_TYPE::SHARDCTRLER;
    }
    KVServer kvserver(sip,sport,zkip,zkport,-1,type);
    while(1){}
    return 0;
}