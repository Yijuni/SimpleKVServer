#include "HostNet.hpp"
#include <iostream>
using namespace std;
int main()
{
    uint32_t n;
    cin >> n;
    std::string res = HostNet::host_to_net_uint32(n);
    n = HostNet::net_to_host_uint32(res);
    return 0;
}