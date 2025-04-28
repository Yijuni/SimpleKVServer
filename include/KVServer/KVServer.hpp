#ifndef KVSERVER_HPP
#define KVSERVER_HPP
#include "KVService.hpp"
#include "Raft.hpp"
#include "LockQueue.hpp"
#include "KVRaft.pb.h"
#include "Persister.hpp"
#include <vector>

class KVServer
{
public:
    KVServer(std::string ip="127.0.0.1",uint16_t port=8009,std::string zkip="127.0.0.1",uint16_t zkport=8010);

private:
    std::string ip_myj;
    uint16_t port_myj;
    std::vector<std::shared_ptr<kvraft::KVRaftRPC_Stub>> raftConnPtr_myj;
    std::shared_ptr<Persister> persister_myj;
};



#endif