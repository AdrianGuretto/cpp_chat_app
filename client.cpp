#include "client.h"

int main(int argc, char* argv[]){
    if (argc != 4){
        std::cerr << "[Usage] ./client <remote_hostname> <port> <nickname>" << std::endl;
        return 1; 
    }
    
    if (strlen(argv[3]) > NICKNAME_MAX_LEN){
        std::cerr << "[ClientError] Nickname length should not exceed 20 characters." << std::endl;
        return 1;
    }

    std::unique_ptr<Client> client;
    try{
        client = std::make_unique<Client>(argv[1], argv[2], argv[3]);
        client->Connect();
    } catch (std::runtime_error& err){
        std::cerr << err.what() << std::endl;
    }
}