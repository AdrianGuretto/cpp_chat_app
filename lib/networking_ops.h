#pragma once
#include <sys/socket.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h> // close()

#include <poll.h>

#include <errno.h>
#include <stdexcept>
#include <string.h>
#include <iostream>

#include <string>

#include "color.h"

using namespace std::string_literals;

struct ConnectionInfo{
    std::string ip_address;
    int port;

    std::string ToString() const noexcept{
        std::string new_str;
        new_str.reserve(65);
        new_str.append("("s).append(ip_address).append(":"s).append(std::to_string(port)).append(")"s);
        return new_str;
    }
};

enum class NicknameAction{
    NICK_PROMPT = 0,
    NICK_ACCEPT = 1,
    NICK_STAKEN = 2,
    NICK_INVALD = 3
};

static const std::unordered_map<NicknameAction, std::string> nickaction_to_keysig_string = {{NicknameAction::NICK_PROMPT, "\07NICK_PROMPT"s}, {NicknameAction::NICK_ACCEPT, "\07NICK_ACCEPT"s}, {NicknameAction::NICK_STAKEN, "\07NICK_STAKEN"s}, {NicknameAction::NICK_INVALD, "\07NICK_INVALD"s}};

/**
 * @param socketfd a socket file descriptor to setsockopt()
 * @param socket_option SO_xxxx 
 * @throw std::runtime_error on setsockopt() -1 return
*/
static void SetSocketOption(int socketfd, int socket_option){
    int yes = 1;
    if (setsockopt(socketfd, SOL_SOCKET, socket_option, &yes, sizeof(yes)) == -1){
        std::string error_msg("setsockopt(): "s + std::string(strerror(errno)));
        
    }
}

/**
 * Convert sockaddr_storage structure to an IPv4 or IPv6 address.
 * @param conn_address pointer to the address-holding structure
*/
static ConnectionInfo GetConnectionInfo(sockaddr_storage* conn_address){
    char address[INET6_ADDRSTRLEN];
    memset(&address, 0, sizeof(address));

    int port;

    if (conn_address->ss_family == AF_INET) { // IPv4
        sockaddr_in* addr_inf = reinterpret_cast<sockaddr_in*>(conn_address);
        inet_ntop(AF_INET, &addr_inf->sin_addr, address, INET_ADDRSTRLEN);
        port = ntohs(addr_inf->sin_port);
    } else if (conn_address->ss_family == AF_INET6) { // IPv6
        sockaddr_in6* addr_inf = reinterpret_cast<sockaddr_in6*>(conn_address);
        inet_ntop(AF_INET6, &addr_inf->sin6_addr, address, INET6_ADDRSTRLEN);
        port = ntohs(addr_inf->sin6_port);
    }
    ConnectionInfo conn_info{.ip_address = address, .port = port};
    
    return conn_info;
}
static ConnectionInfo GetConnectionInfoFromSocket(int socketfd){
    sockaddr_storage addr_inf;
    socklen_t addr_inf_len = sizeof(addr_inf);
    getpeername(socketfd, reinterpret_cast<sockaddr*>(&addr_inf), &addr_inf_len);
    return GetConnectionInfo(&addr_inf);
}


// Pack a message into a communication packet: <msg_len><msg>
static std::string AssembleMessagePacket(std::string&& original_message){
    int orig_msg_len = original_message.size();
    std::string assembled_msg(std::to_string(orig_msg_len));
    assembled_msg.reserve(4 + orig_msg_len); 
    while (assembled_msg.size() < 4){ // 4 bytes = 4 digits in MESSAGE_MAX_LEN
        assembled_msg = '0' + assembled_msg;
    }
    assembled_msg.append(std::move(original_message));
    return assembled_msg;
}
static std::string AssembleMessagePacket(const std::string& original_message){
    int orig_msg_len = original_message.size();
    std::string assembled_msg(std::to_string(orig_msg_len));
    assembled_msg.reserve(4 + orig_msg_len); 
    while (assembled_msg.size() < 4){ // 4 bytes = 4 digits in MESSAGE_MAX_LEN
        assembled_msg = '0' + assembled_msg;
    }
    assembled_msg.append(original_message);
    return assembled_msg;
}

/**
 * SendMessage's internal-use method. Makes sure that all message bytes are sent.
 * @param receiver_socketfd a socket we are sending the message to
 * @param msg_buffer a pointer to a message string storage
 * @param message_len length of the message to be delivered
 * @throw std::runtime_error on send() -1 return
*/
static int __SendAllBytes__(int receiver_socketfd, const char* msg_buffer, size_t message_length){
    int total = 0; // how many bytes we've sent
    int left_bytes = message_length; // how many bytes are left to send
    int sent_bytes_n;
    while (total < message_length){
        sent_bytes_n = send(receiver_socketfd, msg_buffer + total, left_bytes, 0);
        if (sent_bytes_n == -1) break;

        total += sent_bytes_n;
        left_bytes -= sent_bytes_n;
    }

    return sent_bytes_n == -1 ? -1 : 0; // -1 on failure, 0 on success
}

/**
 * Convert a message to the format <msg_length><msg> and send it to another socket.
 * @param receiver_socketfd a socket to where the message will be sent.
 * @param message data to be sent.
 * @return 0 on success, -1 on error with errno set. 
*/
static int SendMessage(int receiver_socketfd, std::string&& message){
    std::cerr << "Sending1: " << message << '\n';
    std::string final_msg(AssembleMessagePacket(std::move(message)));
    std::cerr << "Sending: " << final_msg << '\n';
    if (__SendAllBytes__(receiver_socketfd, final_msg.data(), final_msg.size()) == -1){
        return -1;
    }
    return 0;
}
static int SendMessage(int receiver_socketfd, const std::string& message){
    std::cerr << "Sending1: " << message << '\n';
    std::string final_msg(AssembleMessagePacket(message));
    std::cerr << "Sending: " << final_msg << '\n';
    if (__SendAllBytes__(receiver_socketfd, final_msg.data(), final_msg.size()) == -1){
        return -1;
    }
    return 0;
}

/**
 * Receive a message packet coming in the format: <msg_length><msg> and write <msg> to message_buffer.
 * @param sender_socketfd socket of a message sender
 * @param message_buffer a buffer to where the received message will to be written
 * @return length of the received message on success, 0 if sender_socketfd has closed the connection, -1 on error with errno set
*/
static int ReceiveMessage(int sender_socketfd, char* message_buffer){
    char msg_len_str[4];
    memset(&msg_len_str, 0, sizeof(msg_len_str));

    int recv_bytes = recv(sender_socketfd, msg_len_str, 4, 0); // recv_msg_length
    if (recv_bytes == -1){
        return -1;
    }
    else if (recv_bytes == 0){
        return 0;
    }

    int msg_len = std::atoi(msg_len_str);

    memset(message_buffer, 0, sizeof(*message_buffer));
    recv_bytes = recv(sender_socketfd, message_buffer, msg_len, 0);
    if (recv_bytes == -1){
        return -1;
    }
    else if (recv_bytes == 0){
        return 0;
    }
    std::cerr << "Received: "s << message_buffer << std::endl;
    return recv_bytes;
}