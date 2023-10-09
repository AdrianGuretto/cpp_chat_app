#pragma once

#include "../../lib/networking_ops.h"

#include <iostream>
#include <memory>

#include "pthread.h"

int EXIT_FLAG = 0;
void InterruptHandler(int signal_num){
    EXIT_FLAG = 1;
}

class Client{
public:
    explicit Client(const char* hostname, const char* port);

    explicit Client(const Client& other) = delete;
    Client& operator=(const Client& other) = delete;

    ~Client();

public: // ---------- MAIN API ----------

    /**
     * Main method for starting the client.
    */
    int Connect();

    /**
     * Main method for shutting down the client.
    */
    void Disconnect() noexcept;

private: // ---------- HELPER METHODS ----------
    int ProcessInputCommand(char* msg_storage_buffer, char* write_buffer);

    static void __OverwriteStdout__() noexcept{
        std::cout << "> "s;
        std::cout.flush();
    }

    void* InputHandler(void);
    void* OutputDisplay(void);

    static void* __InputHaldlerHelper__(void* context){
        return ((Client*)context)->InputHandler();
    }
    static void* __OutputDisplayHandlerHelper__(void* context){
        return ((Client*)context)->OutputDisplay();
    }

private:
    const std::string remote_host_address_, remote_host_port_;
    int client_socket_;

};

Client::Client(const char* hostname, const char* port) : remote_host_address_(hostname), remote_host_port_(port) {}

Client::~Client(){
    Disconnect();
}

int Client::Connect(){
    std::cerr << MakeColorfulText("[ClientInit] Initializing the client..."s, Color::Yellow) << '\n';

    addrinfo hints, *res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    signal(SIGINT, )
    
    // Fill out the address structure for creating client's socket
    {
        int getaddrinfo_status_code = getaddrinfo(remote_host_address_.data(), remote_host_port_.data(), &hints, &res);
        if (getaddrinfo_status_code != 0){
            throw std::runtime_error("getaddrinfo(): "s + std::string(gai_strerror(getaddrinfo_status_code)));
        }
    }

    // Create client's socket
    client_socket_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (client_socket_ == -1){
        throw std::runtime_error("socket(): "s + std::string(strerror(errno)));
    }

    std::cerr << MakeColorfulText("[ClientInit] Successfully initialized the client."s, Color::Green);

    // Connect to the remote host.
    std::cerr << MakeColorfulText("[Connect] Trying to connect to "s + remote_host_address_ + ":"s + remote_host_port_, Color::Yellow);
    if (connect(client_socket_, res->ai_addr, res->ai_addrlen) == -1){
        throw std::runtime_error("connect(): "s + std::string(strerror(errno)));
    }

    std::cerr << MakeColorfulText("[Connect] Connected to the remote host. Authorizing..."s, Color::Pink) << '\n';
    freeaddrinfo(res);

    // Creating two working threads for input and output
    pthread_t input_reader_thread, output_display_thread;
    if (pthread_create(&input_reader_thread, NULL, __InputHaldlerHelper__, this) != 0){
        throw std::runtime_error("Failed to start input thread: pthread_create(): "s + std::string(strerror(errno)));
    }

    if (pthread_create(&output_display_thread, NULL, __OutputDisplayHandlerHelper__, this) != 0){
        throw std::runtime_error("Failed to start message-display thread: pthread_create(): "s + std::string(strerror(errno)));
    }

    while (EXIT_FLAG != 1){

    }
    Disconnect();
}

void Client::Disconnect() noexcept{
    std::cerr << MakeColorfulText("[Disconnect] Disconnecting..."s, Color::Pink) << '\n';
    close(client_socket_);
    std::cerr << MakeColorfulText("[Disconnect] Successfully disconnected from the server!"s, Color::Pink) << '\n'
}

int Client::ProcessInputCommand(std::string& msg_str, char* write_buffer){
    // TO DO
}

void* Client::InputHandler(void){
    char write_buffer[1068]; // 44 bytes for message headers
    while (true){
        fgets(write_buffer, 1024, stdin);

        std::string msg_str(write_buffer);

        if (SendMessage(client_socket_, std::string(write_buffer)) == -1){
            throw std::runtime_error("Failed to send message: send(): "s + std::string(strerror(errno)));
        }
    }
}
void* Client::OutputDisplay(void){
    char write_buffer[1068]; // 44 bytes for message headers
}