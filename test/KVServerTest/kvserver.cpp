#include "KVServer.hpp"
#include <iostream>
#include <string>
int main(int argc,char **argv){
    if(argc<5){
        std::cout<<"格式错误:./* ip port zkip zkport"<<std::endl;
        return 0;
    }
    std::string ip(argv[1]);
    uint16_t port = std::atoi(argv[2]);
    std::string zkip(argv[3]);
    uint16_t zkport = std::atoi(argv[4]);
    KVServer kvserver(ip,port,zkip,zkport,512);

    while(1){}
    return 0;
}