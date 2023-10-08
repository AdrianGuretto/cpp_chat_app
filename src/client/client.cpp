#include "client.h"

int main(int argc, char* argv[]){
    if (argc != 3){
        std::cerr << "[Usage] ./client <remote_host> <port>" << std::endl;
        return 1;
    }
}