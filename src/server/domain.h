// This file contains all server-specific structures
#pragma once

#include <string>

struct User{
    std::string nickname;
    std::string ip_address;
    std::string port;
};

struct DisconnectedClient{
    int socket_fd;
    std::string disconnect_reason;
};

enum class ClientKeySignal{
    NICK_NEWREQ = 0,
    ACT_NICKCNG = 1,
    ACT_LSUSERS = 2,
    ACT_PMSGUSR = 3,
    UNKNOWN = 4
};

static ClientKeySignal StringToClientKeySignal(const std::string& command_str){
    if (command_str == "NICK_NEWREQ"s){
        return ClientKeySignal::NICK_NEWREQ;
    } else if (command_str == "ACT_NICKCNG"s){
        return ClientKeySignal::ACT_NICKCNG;
    } else if (command_str == "ACT_LSUSERS"s){
        return ClientKeySignal::ACT_LSUSERS;
    } else if (command_str == "ACT_PMSGUSR"s){
        return ClientKeySignal::ACT_PMSGUSR;
    } else{
        return ClientKeySignal::UNKNOWN;
    }
}