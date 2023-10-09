#include "client.h"

int main(int argc, char* argv[]){
    if (argc != 3){
        std::cerr << "[Usage] ./client <remote_host> <port>" << std::endl;
        return 1;
    }

   std::unique_ptr<Client> client = std::make_unique<Client>(argv[1], argv[2]);
    try{
        client->Connect();
    } catch(std::runtime_error& err){
        std::cerr << MakeColorfulText(err.what(), Color::Red);
        return 1;
    }

    return 0;
}