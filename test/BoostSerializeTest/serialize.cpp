#include <boost/serialization/map.hpp>
#include <boost/serialization/string.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/vector.hpp>
#include "KVRaft.pb.h"
#include <vector>
#include <sstream>
int main(){
    std::string test("dadad\0dadada",11);
    std::cout<<test<<std::endl;
    std::vector<kvraft::LogEntry> entries;
    for(int i=0;i<5;i++){
        kvraft::LogEntry log;
        log.set_term(i);
        kvraft::Command *command =  log.mutable_command();
        command->set_clientid("ba\0ka",5);
        command->set_key("111");
        command->set_type("GET");
        command->set_value("99332");
        std::cout<<"term:"<<i<<" type:"<<command->type()<<" id: "<<command->clientid()<<" key: "<<command->key()<<" value: "<<command->value()<<std::endl;
        entries.emplace_back(log);
    }
    std::vector<std::string> vec;
    for(int i=0;i<5;i++){
        std::string str;
        entries[i].SerializeToString(&str);
        vec.emplace_back(str);
    }
    std::ostringstream oss;
    boost::archive::binary_oarchive bos(oss);
    bos<<vec;
    std::string serdata = oss.str();
    std::cout<<"序列后的数据:"<<serdata<<" size:"<<serdata.size()<<std::endl;
    std::istringstream iss(serdata);
    boost::archive::binary_iarchive bis(iss);
    std::vector<std::string> dvec;
    std::vector<kvraft::LogEntry> dentries;
    bis>>dvec;
    for(int i=0;i<5;i++){
        kvraft::LogEntry log;
        log.ParseFromString(dvec[i]);
        dentries.emplace_back(log);
        std::cout<<"term: "<<log.term()<<" type:"<<log.command().type()<<" id: "<<log.command().clientid()<<" key: " <<log.command().key()<<" value: "<<log.command().value()<<std::endl;
    }

    return 0;
}