#include "MakeServerStub.hpp"

int main(int argc,char **argv){
    if(argc < 3){
        std::cout<<"格式错误:./* zkip zkport"<<std::endl;
        return 0;
    }
    std::string ip(argv[1]);
    uint16_t port = atoi(argv[2]);
    MakeServerStub makeserverstub(ip,port);
    while (1)
    {
        
    }
    
    return 0;
}