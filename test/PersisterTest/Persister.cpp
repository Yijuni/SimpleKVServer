#include <iostream>
#include "Persister.hpp"
int main(){
    Persister persister;
    while (1)
    {
        std::string name;
        std::cin>>name;
        std::cout<<name<<std::endl;

        std::string other;
        std::cin>>other;
        std::cout<<other;
        persister.Save(name,other);
        
        std::cout<<"raftstate:"<<persister.ReadRaftState()<<" size:"<<persister.RaftStateSize()<<" snapshot:"<<persister.ReadSnapshot()<<" size:"<<persister.SnapshotSize()<<std::endl;
    }
    
    
    return 0;
}