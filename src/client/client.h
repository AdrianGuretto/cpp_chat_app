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
    int ProcessInputCommand(std::string&& command_str, char* write_buffer);

    static void __OverwriteStdout__() noexcept{
        std::cout << "> "s;
        std::cout.flush();
    }

    /**
     * Method for handling user input on a separate thread.
    */
    void* InputHandler(void);
    /**
     * Method for handling incoming messages on a separate thread.
    */
    void* OutputDisplay(void);

    /**
     * Helper method for establising a InputHandler thread.
    */
    static void* __InputHaldlerHelper__(void* context){
        return ((Client*)context)->InputHandler();
    }
    /**
     * Helper method for establishing a OutputHandler's thread.
    */
    static void* __OutputDisplayHandlerHelper__(void* context){
        return ((Client*)context)->OutputDisplay();
    }
    /**
     * Main method for establishing connection with the server.
     * @return 0 on success, -1 on error.
    */
    int EstablishConnection();

    /**
     * Process a received from the server message.
     * @param read_buffer a string buffer to read from and write the message to.
     * @return 0 on success, -1 on error with errno set.
    */
   int ProcessMessage(char* read_buffer);

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

    signal(SIGINT, InterruptHandler);
    
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

    std::cerr << MakeColorfulText("[ClientInit] Successfully initialized the client."s, Color::Green) << '\n';

    // Connect to the remote host.
    std::cerr << MakeColorfulText("[Connect] Trying to connect to "s + remote_host_address_ + ":"s + remote_host_port_, Color::Yellow) << '\n';
    if (connect(client_socket_, res->ai_addr, res->ai_addrlen) == -1){
        throw std::runtime_error("connect(): "s + std::string(strerror(errno)));
    }

    std::cerr << MakeColorfulText("[Connect] Connected to the remote host. Authorizing..."s, Color::Pink) << '\n';
    freeaddrinfo(res);

    if (EstablishConnection() != 0){
        throw std::runtime_error("Failed to establish a connection to the server.\n"s);
    }

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
    return 0;
}

void Client::Disconnect() noexcept{
    std::cerr << MakeColorfulText("[Disconnect] Disconnecting..."s, Color::Pink) << '\n';
    close(client_socket_);
    std::cerr << MakeColorfulText("[Disconnect] Successfully disconnected from the server!"s, Color::Pink) << '\n';
}

int Client::ProcessInputCommand(std::string&& command_str, char* write_buffer){
    std::string command_name(command_str.substr(0, 11));
    if (command_name == "quit"s){
        Disconnect();
    }
    else if (command_name == "list_users"s){
        SendMessage(client_socket_, "\07ACT_LSUSERS"s);
        std::string serv_reply;
        serv_reply.reserve(1068);

        ReceiveMessage(client_socket_, serv_reply.data());

        // parsing a string
        std::string token;
        size_t pos;
    }
    else if (command_name == "change_name"s){
        //
    }
    return 1;
}

void* Client::InputHandler(void){
    char write_buffer[1068]; // 44 bytes for message headers
    memset(&write_buffer, 0, sizeof(write_buffer));
    while (true){
        fgets(write_buffer, 1024, stdin);

        std::string msg_str(write_buffer);

        if (SendMessage(client_socket_, std::string(write_buffer)) == -1){
            throw std::runtime_error("Failed to send message: send(): "s + std::string(strerror(errno)));
        }
    }
}

int Client::ProcessMessage(char* read_buffer){
    return 0;
}

void* Client::OutputDisplay(void){
    char write_buffer[1068]; // 44 bytes for message headers
    memset(&write_buffer, 0, sizeof(write_buffer));

    while (true){
        ReceiveMessage(client_socket_, write_buffer);
        if (ProcessMessage(write_buffer) == -1){
            throw std::runtime_error("Failed to process message from the server: "s + std::string(strerror(errno)));
        }
        std::cout << write_buffer << '\n';
    }
}

int Client::EstablishConnection(){
    char writable_buffer[1068]; // 44 bytes for message headers
    memset(&writable_buffer, 0, sizeof(writable_buffer));

    if (ReceiveMessage(client_socket_, writable_buffer) == -1){
        std::cerr << MakeColorfulText("[Error] EstablishConnection: ReceiveMessage() fail: "s + std::string(strerror(errno)), Color::Red);
        return -1;
    }
    std::string key_signal_name(writable_buffer + 1); // +1 to skip the key signal character (\07)
    if (key_signal_name != "NICK_PROMPT"){
        std::cerr << MakeColorfulText("[Error] EstablishConnection(): Received a wrong initial signal: "s + key_signal_name, Color::Red) << '\n';
        return -1;
    }

    while (true){
        std::string nick_str;
        std::cout << "Enter your nickname\n"s;
        __OverwriteStdout__();
        std::getline(std::cin, nick_str);
        if (nick_str.find(' ') != nick_str.npos || nick_str.size() > 20){ // if a space is found or nickname is more than 20 chars
            __OverwriteStdout__();
            std::cerr << MakeColorfulText("[Error] Entered name contains a space or is more than 20 characters. Try again."s, Color::Red) << '\n';
            continue;
        }

        // Get response from the server about our nickname
        SendMessage(client_socket_, "\07NICK_NEWREQ"s + std::move(nick_str));
        
        std::string serv_response;
        serv_response.reserve(250);
        ReceiveMessage(client_socket_, serv_response.data());
        std::cerr << "Received response from the server"s << '\n';

        std::string server_command(serv_response.substr(1)); // omit the first key signal char
        if (server_command == "NICK_ACCEPT"s){
            std::cout << "[SERVER] "s << server_command << '\n';
            std::cerr << MakeColorfulText("[Connection] Connected to the server."s, Color::Green) << '\n';
            return 0;
        } else if (server_command == "NICK_STAKEN"s){
            std::cerr << MakeColorfulText("[NickRefused] Entered nickname is already taken. Enter a new one."s, Color::Red);
            continue;
        } else if (server_command == "NICK_INVALD"s){
            std::cerr << MakeColorfulText("[NickRefused] Entered nickname contains forbidden characters. Enter a new one.", Color::Red);
        }
        else{ // Unknown key signal
            std::cerr << MakeColorfulText("[Error] EstablishConnection(): Received an unknown key signal: "s + std::string(serv_response), Color::Red) << '\n';
            return -1; 
        }
    }

    return 0;
}