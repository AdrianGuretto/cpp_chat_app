#include <iostream>

#define START_OF_HEADER_CHAR '\07'

int main(){
    std::string key_message;
    key_message += START_OF_HEADER_CHAR;
    key_message += "DISCONNECT";
    if (key_message[0] == START_OF_HEADER_CHAR){
        std::cout << "Reading Key message: " << key_message.substr(1) << std::endl;
    }
    else{
        std::cout << "Reading Regular message: " << key_message.substr(1) << std::endl;
    }
}