#include "KVRpcChannel.hpp"
#include "KVRpcController.hpp"
#include "KVService.pb.h"
#include "ZKClient.hpp"
#include "KVClient.hpp"

int main()
{
    KVClient client("127.0.0.1", 2181,"client1");
    while (1)
    {
        std::string type;
        std::string key;
        std::string value;
        std::cin >> type;
        std::cin >> key;
        std::cin >> value;
        if (type == "Get")
        {
            std::string value;
            if (client.Get(key, value))
            {
                std::cout << "Get [" << key << "] " << "value [" << value << "]" << std::endl;
            }
            else
            {
                std::cout << "Get请求失败" << std::endl;
            }
        }
        else if (type == "Put")
        {

            if (client.Put(key, value))
            {
                std::cout << "Put [" << key << "] " << "value [" << value << "] 成功" << std::endl;
            }
            else
            {
                std::cout << "Put请求失败" << std::endl;
            }
        }
        else
        {
            if (client.Append(key, value))
            {
                std::cout << "Append [" << key << "] " << "value [" << value << "] 成功" << std::endl;
            }
            else
            {
                std::cout << "Append请求失败" << std::endl;
            }
        }
    }

    return 0;
}