#ifndef HOSTNET_H
#define HOSTNET_H
/**
 * 2025-4-22 moyoj
 * 自己实现的本机uint32_t转网络string，网络string转本机uint32_t
 **/

#include <string>
class HostNet
{
public:
    static std::string host_to_net_uint32(uint32_t number);
    static uint32_t net_to_host_uint32(std::string str);
    static bool isBigEndian();
};

#endif