#include "KVRpcChannel.hpp"
#include "KVRpcController.hpp"
#include "KVService.pb.h"
// int main(){
//     kvservice::KVServiceRPC_Stub stub(new KVRpcChannel());
//     kvservice::GetRequest request;
//     kvservice::GetResponse response;
//     std::cout<<"成功连接"<<std::endl;
//     while(1){
//         string str;
//         std::cin>>str;
//         request.set_key(str);
//         KVRpcController controller;
//         stub.Get(&controller,&request,&response,nullptr);
//         if(controller.Failed()){
//             std::cout<<controller.ErrorText();
//             return 0;
//         }
//         std::cout<<"code:"<<response.resultcode().errorcode()<<" msg:"<<response.resultcode().errormsg()<<std::endl;
//     }

//     return 0;
// }