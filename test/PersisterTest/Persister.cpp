#include <iostream>
#include "Persister.hpp"
int main(){
    Persister persister;
    std::string name("baka!\0w666",10);
    std::cout<<name.size()<<std::endl;
    std::string other = "snapshot";
    persister.Save(name,other);
    std::cout<<"raftstate:"<<persister.ReadRaftState()<<" size:"<<persister.RaftStateSize()<<" snapshot:"<<persister.ReadSnapshot()<<" size:"<<persister.SnapshotSize()<<std::endl;
    return 0;
}