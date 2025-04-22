#include "HostNet.hpp"
#include <algorithm>
std::string HostNet::host_to_net_uint32(uint32_t number)
{
    if(isBigEndian()){
        return std::string((char*)&number,4);
    }else{
        std::string str = std::string((char*)&number,4);
        reverse(str.begin(),str.end());
        return str;
    }
}

uint32_t HostNet::net_to_host_uint32(std::string str)
{
    if(!isBigEndian()){
        reverse(str.begin(),str.end());
    }
    uint32_t header_size = 0;
    str.copy((char*)&header_size,4,0);
    return header_size;
}

bool HostNet::isBigEndian()
{
    unsigned int x = 1;
    char *c = (char*)&x;
    return (*c) ? false : true;
}
