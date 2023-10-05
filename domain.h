#pragma once

#include <string>

struct NewConnectionInfo{
    int socketfd;
    std::string ip_address;
    std::string port;
};

struct User{
    std::string name;
    std::string ip_address;
    std::string port;
};