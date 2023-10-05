#pragma once

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include <thread>

#include <netdb.h>
#include <errno.h>
#include <unistd.h>

#include <iostream>

#define NICKNAME_MAX_LEN 20
#define MESSAGE_MAX_LEN 2048

// Used for properly handling user input
void OverwriteStdout(){
    std::cout << '\n';
    std::cout.flush();
    std::cerr.flush();
}



class Client{
public: // --------- CONSTRUCTORS/OPERATORS ---------
    explicit Client(char* hostname, char* port, char* nickaname);

    explicit Client(const Client& other) = delete;
    explicit Client(Client&& other) = delete;

    Client& operator=(const Client& other) = delete;
    Client& operator=(Client&& other) = delete;

public: // --------- MAIN API ---------

    /**
     * Main method for launching the client.
    */
    void Connect();

    /**
     * Main method for shutting down the client.
    */
    void Disconnect();

private: // --------- HELPER METHODS ---------

    /**
     * Method for crafting a message for sending to the server in this format: <msg_len><msg>
    */
    static std::string AssembleMessage(std::string&& orig_message);

    /**
     * Receive a message from the server.
     * @param buf a buffer to write a received data
    */
    void ReceiveMessage(char* buf);

    /**
     * Internal method for SendMessage. Used for safe message sending with all message bytes delivered.
     * @param buf a message buffer to read from
     * @param message_len length of the message to be sent
    */
    void SendAllBytes(const char* buf, const size_t message_len) const;

    /**
     * Main message-sending method for communicating with the server.
     * @param message a message to be sent to the server.
    */
    void SendMessage(std::string&& message) const;


    /**
     * A method for establishing initial connection with the server.
    */
    void EstablishConnection();

    /**
     * Manages receiving messages from the server.
    */
    void MessageReceiveHandler();

    /**
     * Manages user input.
    */
    void MessageSendHandler();

private:
    const char* hostname_, port_;
    std::string nickname_;
    addrinfo* remote_host_info_ = NULL;

    int client_socket_;
};

Client::Client(char* hostname, char* port, char* nickname) : hostname_(hostname), port_(port), nickname_(nickname) {
    addrinfo hints;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    { // fill addrinfo structure for creating socket later on
        int getaddinfo_status_code = getaddrinfo(hostname, port, &hints, &remote_host_info_);
        if (getaddinfo_status_code != 0){
            throw std::runtime_error("[ClientInit] Failed to initialize the client: getaddrinfo(): " + std::string(gai_strerror(getaddinfo_status_code)));
        }
    }

    // create a client socket
    if ((client_socket_ = socket(remote_host_info_->ai_family, remote_host_info_->ai_socktype, remote_host_info_->ai_protocol)) == -1){
        throw std::runtime_error("[ClientInit] Failed to initialize the client: socket(): " + std::string(strerror(errno)));
    }

    std::cerr << "[ClientInit] Successfully configured the client.\n";
}

// --------- HELPER METHODS ---------

std::string Client::AssembleMessage(std::string&& orig_message){
    std::string msg_len_str(std::to_string(orig_message.size()));
    while (msg_len_str.size() < 4){ // MESSAGE_MAX_SIZE has 4 digits
        msg_len_str = '0' + msg_len_str;
    }
    return std::string(msg_len_str + std::move(orig_message));
}

void Client::ReceiveMessage(char* buf){
    
    char msg_len_str[4];
    int recved_bytes;
    auto error_checking_func = [&](){
        if (recved_bytes <= 0){
            if (recved_bytes == 0){ // the client's socket was shutdown
                std::cerr << "[ServerClosed] Server closed the connection.\n";
                Disconnect();
                memset(buf, 0, strlen(buf)); // clear the message buffer
            }
            else{ // an error has occurred
                throw std::runtime_error("[ReceiveMessage] Failed to receive message from the server: recv(): " + std::string(strerror(errno)));
            }
        }
    };
    // Receive message header (length)
    recved_bytes = recv(client_socket_, msg_len_str, 4, 0);
    error_checking_func();


    int msg_len = std::atoi(msg_len_str);
    // Receive the actual message
    recved_bytes = recv(client_socket_, buf, static_cast<size_t>(msg_len), 0);
    error_checking_func();
}

void Client::SendAllBytes(const char* buf, const size_t message_len) const{
    size_t total = 0; // how many bytes we've sent
    size_t bytes_left = message_len; // how many more bytes we have to send;
    int n; // tmp var

    while (total < message_len){
        n = send(client_socket_, buf + total, bytes_left, 0);
        if (n == -1){
            throw std::runtime_error("[SendAllBytes] Failed to send message: send(): " + std::string(strerror(errno)));
        }
        total += n;
        bytes_left -= n;
    }
}

void Client::SendMessage(std::string&& message) const{
    std::string assembled_message = AssembleMessage(std::move(message));
    SendAllBytes(assembled_message.data(), assembled_message.size());
}

void Client::EstablishConnection(){
    std::string server_signal;
    ReceiveMessage(server_signal.data());
    if (server_signal == "NICK_PROMPT"){
        while (true){
            server_signal.clear();
            SendMessage(nickname_.data());
            ReceiveMessage(server_signal.data());
            if (server_signal == "NICK_OK"){
                return;
            }
            else if (server_signal == "NICK_INVALCHAR" || server_signal == "NICK_TAKEN"){
                std::cerr << "[ServerConnection] Current nickname is invalid. Enter a new one (20 char max): ";
                std::cerr.flush();
                nickname_.clear();
                std::cin >> nickname_;
            }
        }
    }
}

void Client::MessageReceiveHandler(){
    std::string message_buff;
    message_buff.reserve(MESSAGE_MAX_LEN);
    while (true){
        OverwriteStdout();
        
    }
}


// --------- MAIN API ---------

void Client::Connect(){
    std::cerr << "[ClientConnect] Connecting to " << hostname_ << ':' << port_ << '\n';

    if (connect(client_socket_, remote_host_info_->ai_addr, remote_host_info_->ai_addrlen) == -1){
        throw std::runtime_error("[ClientConnect] Failed to connect to remote host: connect(): " + std::string(strerror(errno)));
    }
    freeaddrinfo(remote_host_info_);



}

void Client::Disconnect(){
    std::cerr << "[ClientDisconnect] Disconnecting...\n";
    close(client_socket_);
    std::cerr << "[ClientDisconnect] Disconnected from the server.";
}