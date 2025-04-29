#include "ZKClient.hpp"
#include <vector>
#include <iostream>
#include <Logger.hpp>
int main(){
    ZKClient zkc("127.0.0.1",2181);
    if(!zkc.Connect()){
        std::cout<<"连接失败"<<std::endl;
    }
    zkc.initChildWatcher("/kvserver/servers",[&](){
        LOG_INFO("path[/kvserver/servers]孩子节点信息发生变化");
        std::vector<std::string> childinfo;
        zkc.registerChildWatcher("/kvserver/servers",childinfo);
        for(int i=0;i<childinfo.size();i++){
            std::string ipport;
            std::string path ="/kvserver/servers/" + childinfo[i]; 
            zkc.getPathData(path,ipport);
            LOG_INFO("path[%s]:ipport[%s]",path.c_str(),ipport.c_str());
        }
    });
    std::vector<std::string> childinfo;
    zkc.registerChildWatcher("/kvserver/servers",childinfo);
    for(int i=0;i<childinfo.size();i++){
        std::string ipport;
        std::string path ="/kvserver/servers/" + childinfo[i]; 
        zkc.getPathData(path,ipport);
        LOG_INFO("path[%s]:ipport[%s]",path.c_str(),ipport.c_str());
    }
    while(1){

    }
    return 0;
}