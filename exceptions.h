#pragma once
 
#include <stdexcept>
#include <string>
#include <sstream>

#include <memory>

#include "domain.h"
 
// TO DO : Better organize errors by their types as well as the error messages they accept and display


// Used to indicate that the server was unable to start.
class ServerInitializationFailed final : public std::exception{
public:
    ServerInitializationFailed(std::string&& failed_call, std::string&& error_string) : error_msg_("[ServerInitError] ") {
        std::stringstream ss;
        ss << std::move(failed_call) << ": " << std::move(error_string);

        error_msg_.append(std::move(ss.str()));
    }
 
    const char* what() {
        return error_msg_.data();
    }
private:
    std::string error_msg_;
};

// --------- CLIENT ERRORS ---------

class ClientError : public std::exception{
public:
    ClientError(std::string&& failed_call, std::string&& error_string, const User& client) : error_msg_("[ClientError] ") {
        std::stringstream ss;
        ss << failed_call << ": " << client.name << '(' << client.ip_address << ':' << client.port << "): " << error_string;
        error_msg_.append(std::move(ss.str()));
    }

    const char* what(){
        return error_msg_.data();
    }
private:
    std::string error_msg_;
};

class AcceptConnectionFail : public ClientError{
public:
    AcceptConnectionFail(std::string&& failed_call, std::string&& error_msg, const User& client)
        : ClientError(std::move(failed_call), std::move(error_msg), client) {}
};

class SendMessageFail : public ClientError{
public:
    SendMessageFail(std::string&& failed_call, std::string&& error_msg, const User& client)
        : ClientError(std::move(failed_call), std::move(error_msg), client) {}
};

class ReceiveMessageFail : public ClientError{
public:
    ReceiveMessageFail(std::string&& failed_call, std::string&& error_msg, const User& client)
        : ClientError(std::move(failed_call), std::move(error_msg), client) {}
};

class EstablishConnectionFail : public ClientError{
public:
    EstablishConnectionFail(std::string&& failed_call, std::string&& error_msg, const User& client)
        : ClientError(std::move(failed_call), std::move(error_msg), client) {}
};

class ClientDisconnectError : public ClientError{
public:
    ClientDisconnectError(std::string&& failed_call, std::string&& error_msg, const User& client)
        : ClientError(std::move(failed_call), std::move(error_msg), client) {}
};
// --------- SERVER ERRORS ---------

class ServerError : public std::exception{
public:
    ServerError(std::string&& failed_call, std::string&& error_msg) : error_msg_("[ServerError] ") {
        std::stringstream ss;
        ss << failed_call << ": " << error_msg;
        error_msg_.append(std::move(ss.str()));
    }

    const char* what(){
        return error_msg_.c_str();
    }
private:
    std::string error_msg_;
};

class BroadcastFail : public ServerError{
public:
    BroadcastFail(std::string failed_call, std::string error_msg)
        : ServerError(std::move(failed_call), std::move(error_msg)) {}
};

class ListenForConnectionsFail : public ServerError{
public:
    ListenForConnectionsFail(std::string failed_to_do, std::string error_msg)
        : ServerError(std::move(failed_to_do), std::move(error_msg)) {}
};