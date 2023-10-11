#pragma once

#include "../../lib/networking_ops.h"

#include <iostream>
#include <memory>

#include <curses.h>

#include <signal.h>
#include <thread>
#include <atomic>

std::atomic_int EXIT_FLAG = 0;
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
    /**
     * Parse and process user input and send it to the server.
     * @param command_str a command string from the user.
     * @return 0 on success, -1 on error with errno set.
    */
    int ProcessInputCommand(std::string&& command_str);

    static void __OverwriteStdout__() noexcept{
        std::cout << "> "s;
        std::cout.flush();
    }

    /**
     * Method for handling user input on a separate thread.
    */
    void InputHandler(void);
    /**
     * Method for handling incoming messages on a separate thread.
    */
    void OutputDisplay(void);

    /**
     * Helper method for establising a InputHandler thread.
    */
    // static void* __InputHaldlerHelper__(void* context){
    //     return ((Client*)context)->InputHandler();
    // }
    // /**
    //  * Helper method for establishing a OutputHandler's thread.
    // */
    // static void* __OutputDisplayHandlerHelper__(void* context){
    //     return ((Client*)context)->OutputDisplay();
    // }
    /**
     * Main method for establishing connection with the server.
     * @return 0 on success, -1 on error.
    */
    int EstablishConnection();

    /**
     * Process a message from the server.
     * @param write_buffer a buffer to read and write the ready-to-display message
     * @return 0 on success, -1 on error with errno set.
    */
   int ProcessMessage(char* write_buffer);

private:
    const std::string remote_host_address_, remote_host_port_;
    int client_socket_;

    bool disconnected = false;

};

Client::Client(const char* hostname, const char* port) : remote_host_address_(hostname), remote_host_port_(port) {}

Client::~Client(){
    if (!disconnected){
        Disconnect();
    }
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
    std::thread input_reading_worker(&Client::InputHandler, this);
    std::thread output_display_worker(&Client::OutputDisplay, this);
    while (EXIT_FLAG == 0) {} // Stay inside this loop until we receive an exit signal
    Disconnect();
    input_reading_worker.join();
    output_display_worker.join();
    return 0;
}

void Client::Disconnect() noexcept{
    std::cerr << MakeColorfulText("[Disconnect] Disconnecting..."s, Color::Pink) << '\n';
    close(client_socket_);
    std::cerr << MakeColorfulText("[Disconnect] Successfully disconnected from the server!"s, Color::Pink) << '\n';
    disconnected = true;
}

int Client::ProcessInputCommand(std::string&& command_str){
    std::string command_name(command_str.substr(1, command_str.find_first_of(' ')));
    if (command_name == "list_users"s){
        SendMessage(client_socket_, "\07ACT_LSUSERS"s);
        std::string serv_reply;
        serv_reply.reserve(1068);

        int recv_msg_status_code;
        if ((recv_msg_status_code = ReceiveMessage(client_socket_, serv_reply.data())) == 0){ // Server closed the connection
            EXIT_FLAG = 1;
        } else if (recv_msg_status_code == -1){ // En error occurred while receiving the data
            EXIT_FLAG = 1;
        }

        // parsing a string
        std::string token;
        size_t pos = 0;
        while ((pos = serv_reply.find('\02', pos)) != serv_reply.npos){ // The response will arrive in this form: "1) Username (X.X.X.X:YYYY)<\02>2) Username (X.X.X.X:YYYY)
            token = serv_reply.substr(0, pos);
            std::cout << token << '\n';
            serv_reply.erase(0, pos + 1); // +1 for '\02' delimeter
        }
        std::cout << serv_reply << '\n';
    }
    else if (command_name == "change_name"s){
        
    }
    return 1;
}

void Client::InputHandler(void){
    char write_buffer[1068]; // 44 bytes for message headers
    while (EXIT_FLAG == 0){
        __OverwriteStdout__();
        memset(&write_buffer, 0, sizeof(write_buffer));
        fgets(write_buffer, 1024, stdin);

        std::string msg_str(write_buffer);
        StipString(msg_str);
        
        if (msg_str.size() > 0){
            if (msg_str[0] == '/'){
                if (msg_str == "/quit"){
                    EXIT_FLAG = 1;
                    break;
                }
                if (ProcessInputCommand(std::move(msg_str)) == -1){
                    EXIT_FLAG = 1;
                    throw std::runtime_error("Failed to process input command: "s + std::string(strerror(errno)));
                }
            } else{
                if (SendMessage(client_socket_, std::move(msg_str)) == -1){
                    if (errno == EBADF || EXIT_FLAG == 1){ // if the socket has been closed and exit code set -> we just leave
                        break;
                    }
                    throw std::runtime_error("Failed to send message to the server: "s + std::string(strerror(errno)));
                }
            }
        }
    }
}

int Client::ProcessMessage(char* write_buffer){
    return 1;
}

void Client::OutputDisplay(void){
    char write_buffer[1068]; // 44 bytes for message headers

    while (EXIT_FLAG == 0){
        memset(&write_buffer, 0, sizeof(write_buffer));
        int recved_msg_status;
        if ((recved_msg_status = ReceiveMessage(client_socket_, write_buffer)) == 0){ // Server closed connection
            EXIT_FLAG = 1;
        } else if (recved_msg_status == -1){
            if (errno == EBADF || EXIT_FLAG == 1){ // if the socket has been closed and exit code set -> we just leave
                break;
            }
            EXIT_FLAG = 1;
            throw std::runtime_error("Failed to receive a message from the server: recv(): "s + std::string(strerror(errno)));
        }
        
        std::cout << write_buffer << '\n';
        __OverwriteStdout__();
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
        if (nick_str.find(' ') != nick_str.npos || nick_str.size() > 20 || *nick_str.begin() == '/'){ // if a space is found or nickname is more than 20 chars or the first char is /
            __OverwriteStdout__();
            std::cerr << MakeColorfulText("[Error] Entered name contains a space or is more than 20 characters or '/' at the beginning. Try again."s, Color::Red) << '\n';
            continue;
        }

        // Get response from the server about our nickname
        SendMessage(client_socket_, "\07NICK_NEWREQ"s + std::move(nick_str));
        
        std::string serv_response;
        serv_response.resize(12);
        int recv_bytes;
        if ((recv_bytes = ReceiveMessage(client_socket_, serv_response.data())) == 0){
            std::cerr << MakeColorfulText("[ConnectionClosed] Server closed the connection."s, Color::Pink) << std::endl;
            return -1;
        } else if (recv_bytes == -1){
            std::cerr << MakeColorfulText("[Error] Failed to receive a message from server: recv(): "s + std::string(strerror(errno)), Color::Red) << std::endl;
            return -1;
        }

        std::string server_command(serv_response.substr(1)); // omit the first key signal char
        if (server_command == "NICK_ACCEPT"s){
            std::cerr << MakeColorfulText("[Connection] Connected to the server."s, Color::Green) << '\n';
            break;
        } else if (server_command == "NICK_STAKEN"s){
            std::cerr << MakeColorfulText("[NickRefused] Entered nickname is already taken. Enter a new one."s, Color::Red) << '\n';
        } else if (server_command == "NICK_INVALD"s){
            std::cerr << MakeColorfulText("[NickRefused] Entered nickname contains forbidden characters. Enter a new one.", Color::Red) << '\n';
        }
        else{ // Unknown key signal
            std::cerr << MakeColorfulText("[Error] EstablishConnection(): Received an unknown key signal: \""s + std::string(serv_response), Color::Red) << "\"\n";
            return -1; 
        }
    }

    return 0;
}