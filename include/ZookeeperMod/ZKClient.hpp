#ifndef ZKCLIENT_HPP
#define ZKCLIENT_HPP
#include <string>
#include <zookeeper/zookeeper.h>
class ZKClient{
public:
    ZKClient(std::string zkip="127.0.0.1",uint16_t zkport=8010);
    void Start();
private:
    std::string zkip_myj;
    uint16_t zkport_myj;

};
#endif