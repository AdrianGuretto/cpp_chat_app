#include "server.h"

#include <optional>

// --------- CONSTRUCTORS/OPERATORS ---------

ChatServer::ChatServer(char* hostname, char* port, int addr_ver) : hostname_(hostname), port_(port) {
    addrinfo hints, *res, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = addr_ver;
    
    { // Translating hostname and port to a set of possible sockaddr structures
        int getaddrinfo_status_code = getaddrinfo(hostname, port, &hints, &res);
        if (getaddrinfo_status_code != 0){
            throw ServerInitializationFailed("getaddrinfo()", std::string(gai_strerror(getaddrinfo_status_code)));
        }
    }

    // Create a server socket
    if ((serv_socket_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1){
        throw ServerInitializationFailed("socket()", std::string(strerror(errno)));
    }

    // Set server socket option to SO_REUSEADDR (Address-in-use error handling)
    {
        int yes = 1;
        if (setsockopt(serv_socket_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != 0){
            throw ServerInitializationFailed("setsockopt()", std::string(strerror(errno)));
        }
    }

    // Bind the socket to sockaddr struct from getaddrinfo() syscall
    if (bind(serv_socket_, res->ai_addr, res->ai_addrlen) != 0){
        throw ServerInitializationFailed("bind()", std::string(strerror(errno)));
    }
    freeaddrinfo(res);

    // Convert the socket to the listenning type
    if (listen(serv_socket_, BACKLOG) != 0){
        throw ServerInitializationFailed("listen()", std::string(strerror(errno)));
    }

    std::cerr << "[ServInit] Server configuration succeeded.\n";
}

ChatServer::~ChatServer(){
    Shutdown();
}

// --------- HELPER METHODS ---------

bool ChatServer::IsValidName(const char* name){
    int char_ascii_val;
    for (size_t i = 0; i < strlen(name); ++i){
        char_ascii_val = static_cast<int>(name[i]);
        if (char_ascii_val < 33 || char_ascii_val > 126){
            return false;
        }
    }
    return true;
}

bool ChatServer::IsAvailableName(const char* name){
    std::string name_str(name);
    return active_users.count(name_str) == 0 ? true : false;
}

std::string ChatServer::AssembleMessage(const std::string& message){
    std::string msg_len_str(std::to_string(message.size()));
    while (msg_len_str.size() < 4){ // MESSAGE_MAX_SIZE has 4 digits
        msg_len_str = '0' + msg_len_str;
    }
    return std::string(msg_len_str + message);
}
std::string ChatServer::AssembleMessage(std::string&& message){
    std::string msg_len_str(std::to_string(message.size()));
    while (msg_len_str.size() < 4){ // MESSAGE_MAX_SIZE has 4 digits
        msg_len_str = '0' + msg_len_str;
    }
    return std::string(msg_len_str + std::move(message));
}

std::string ChatServer::AssembleClientMessage(int sender_socketfd, const std::string& message){
    std::string assembled_msg = "[" + (sender_socketfd != serv_socket_ ? socket_to_user_.at(sender_socketfd).name : "SERVER") + "] " + message;   
    std::string msg_len_str(std::to_string(assembled_msg.size()));
    while (msg_len_str.size() < 4){ // MESSAGE_MAX_SIZE has 4 digits
        msg_len_str = '0' + msg_len_str;
    }
    return std::string(msg_len_str + assembled_msg);
}
std::string ChatServer::AssembleClientMessage(int sender_socketfd, std::string&& message){
    std::string assembled_msg = "[" + (sender_socketfd != serv_socket_ ? socket_to_user_.at(sender_socketfd).name : "SERVER") + "] " + message; 
    std::string msg_len_str(std::to_string(assembled_msg.size()));
    while (msg_len_str.size() < 4){ // MESSAGE_MAX_SIZE has 4 digits
        msg_len_str = '0' + msg_len_str;
    }
    return std::string(msg_len_str + std::move(assembled_msg));
}

void ChatServer::SendAllBytes(int socketfd, const char* buf, size_t len){
    size_t total = 0; // how many bytes we've sent
    size_t bytes_left = len; // how many more bytes we have to send;
    int n; // tmp var

    while (total < len){
        n = send(socketfd, buf + total, bytes_left, 0);
        if (n == -1){
            throw SendMessageFail("SendAllBytes, send()", strerror(errno), socket_to_user_.at(socketfd));
        }
        total += n;
        bytes_left -= n;
    }
}

void ChatServer::SendMessageClient(int socketfd, const std::string& message){
    std::string assembled_msg = AssembleClientMessage(socketfd, message);
    SendAllBytes(socketfd, assembled_msg.data(), assembled_msg.size());
}
void ChatServer::SendMessageClient(int socketfd, std::string&& message){
    size_t msg_len = message.size();
    std::string assembled_msg = AssembleClientMessage(socketfd, std::move(message));
    SendAllBytes(socketfd, assembled_msg.data(), assembled_msg.size());
}

void ChatServer::SendMessage(int socketfd, std::string&& message){
    size_t msg_len = message.size();
    std::string crafted_msg = AssembleMessage(std::move(message));
    SendAllBytes(socketfd, crafted_msg.data(), crafted_msg.size());
}

void ChatServer::ReceiveMessage(int socketfd, char* buf){
    char msg_len_str[4];
    int recved_bytes;
    auto error_checking_func = [&](){
        if (recved_bytes <= 0){
            if (recved_bytes == 0){ // the client's socket was shutdown
                Disconnect(socketfd);
                memset(buf, 0, strlen(buf)); // clear the message buffer
            }
            else{ // an error has occurred
                throw ReceiveMessageFail("ReceiveMessage, receive()", strerror(errno), socket_to_user_.at(socketfd));
            }
        }
    };
    // Receive message header (length)
    recved_bytes = recv(socketfd, msg_len_str, 4, 0);
    error_checking_func();


    int msg_len = std::atoi(msg_len_str);
    // Receive the actual message
    recved_bytes = recv(socketfd, buf, static_cast<size_t>(msg_len), 0);
    error_checking_func();
}

void ChatServer::HandleMessage(int sender_socketfd, const std::string& message){
    if (message.size() > 0){
        if (message[0] == '/'){ // it is a command
            std::string command = message.substr(1);
            if (command == "quit"){
                Disconnect(sender_socketfd);
            } else if (command == "change_username"){
                std::string new_username = message.substr(message.find_first_of(' ') + 1);
                if (IsValidName(new_username.data()) && IsAvailableName(new_username.data())){
                    std::string old_username = socket_to_user_.at(sender_socketfd).name;
                    active_users.erase(old_username);
                    active_users.insert(new_username);

                    socket_to_user_.at(sender_socketfd).name = new_username;
                    BroadcastMessage(serv_socket_, old_username + " is now " + new_username);
                }
            }
            else{
                SendMessageClient(sender_socketfd, "Unknown Command: '" + message + "'!");
            }
        }
    }
}

void ChatServer::BroadcastMessage(int sender_socketfd, const std::string& message){
    int poll_count = poll(poll_objects_.data(), poll_objects_.size(), -1); // see if there are available for I/O operations sockets
    if (poll_count == -1){
        throw BroadcastFail("poll()", strerror(errno));
    }
    for (const pollfd& poll_obj : poll_objects_){
        if (poll_obj.revents & POLLOUT){ // check if the client is ready to receive the message
            if (poll_obj.fd == sender_socketfd){
                continue;
            }
            SendMessageClient(poll_obj.fd, message);
        }
    }
}
void ChatServer::BroadcastMessage(int sender_socketfd, std::string&& message){
    int poll_count = poll(poll_objects_.data(), poll_objects_.size(), -1); // see if there are available for I/O operations sockets
    if (poll_count == -1){
        throw BroadcastFail("poll() failed", strerror(errno));
    }
    for (const pollfd& poll_obj : poll_objects_){
        if (poll_obj.revents & POLLOUT){ // check if the client is ready to receive the message
            if (poll_obj.fd == sender_socketfd){
                continue;
            }
            SendMessageClient(poll_obj.fd, std::move(message));
        }
    }
}

NewConnectionInfo ChatServer::AcceptNewConnection(sockaddr* addr, socklen_t* addr_len){
    int conn_socketfd = accept(serv_socket_, addr, addr_len);
    std::string ip, port;
    
    inet_ntop(AF_INET, (sockaddr_in*)addr, ip.data(), *addr_len);
    port = std::to_string(ntohl(((sockaddr_in*)addr)->sin_port));

    NewConnectionInfo new_conn{.socketfd = conn_socketfd, .ip_address = std::move(ip), .port = std::move(port)};
    return new_conn;
}

User ChatServer::EstablishConnection(const NewConnectionInfo& conn_info){
    User user_conn_info;
    try{ // If any error occurrs, the connection cannot be established
        char name[20];
        while (true){
            SendMessage(conn_info.socketfd, "NICK_PROMPT");
            ReceiveMessage(conn_info.socketfd, name);
            if (!IsValidName(name)){
                SendMessage(conn_info.socketfd, "NICK_INVALCHAR");
            } else if (!IsAvailableName(name)){
                SendMessage(conn_info.socketfd, "NICK_TAKEN");
            } else{
                SendMessage(conn_info.socketfd, "NICK_OK");
                break;
            }
            user_conn_info.name = std::string(name);
        }
    } catch (SendMessageFail& err){ 
        throw;
    } catch (ReceiveMessageFail& err){
        throw;
    }

    return user_conn_info;
}

void ChatServer::AddNewUserConnection(const NewConnectionInfo& conn_info){
    User new_user;
    try{
        new_user = EstablishConnection(conn_info);
    } catch (std::runtime_error& err){
        throw;
    }

    // If the connection has been established, we add the user to users list and to poll_objects_
    pollfd new_user_pollfd;
    new_user_pollfd.fd = conn_info.socketfd;
    new_user_pollfd.events = POLLIN | POLLOUT; // Set the active events to Ready-For-Reading and Ready-For-Writing
    poll_objects_.push_back(std::move(new_user_pollfd));
    socket_to_user_[conn_info.socketfd] = std::move(new_user);

    active_users.insert(socket_to_user_.at(conn_info.socketfd).name);

    std::stringstream ss;
    ss << new_user.name << '(' << new_user.ip_address << ':' << new_user.port << ") has joined the server.";
    BroadcastMessage(serv_socket_, std::move(ss.str()));
}

void ChatServer::Disconnect(int socketfd){
    std::string username = socket_to_user_.at(socketfd).name;
    BroadcastMessage(serv_socket_, username + " has been disconnected.");
    active_users.erase(username);
    
    poll_objects_.erase(std::remove_if(poll_objects_.begin(), poll_objects_.end(), [socketfd](const pollfd& poll_obj){
        return poll_obj.fd == socketfd;
    }), poll_objects_.end());

    socket_to_user_.erase(socketfd);
}


// --------- MAIN API ---------

void ChatServer::Launch(){
    // Add the first pollfd object, server socket, that will notify when it is ready to be read from.
    // If the serversocket pollfd is ready to be read from, it means that there is an incoming connection
    {
        pollfd listenner_pfd;
        listenner_pfd.fd = serv_socket_;
        listenner_pfd.events = POLLIN; // Set the notifying event to POLLIN (Ready-To-Read-From)
        poll_objects_.push_back(std::move(listenner_pfd));
    }

    std::cerr << "[Server] Listenning for incoming connections on " << hostname_ << ':' << port_ << std::endl;

    sockaddr_storage conn_remote_addr;
    socklen_t remote_addr_len;

    std::string msg_buffer;
    msg_buffer.reserve(MESSAGE_MAX_SIZE);
    while (true){
        int poll_count = poll(poll_objects_.data(), poll_objects_.size(), -1);
        if (poll_count == -1){
            throw ListenForConnectionsFail("poll()", strerror(errno));
        }

        for (const pollfd& poll_obj : poll_objects_){
            if (poll_obj.revents & POLLIN){ // check if someone is ready for reading from
                if (poll_obj.fd == serv_socket_){ // server socket is ready-to-read = new connection
                    remote_addr_len = sizeof(conn_remote_addr);
                    NewConnectionInfo new_conn = AcceptNewConnection((sockaddr*)&conn_remote_addr, &remote_addr_len);
                    if (new_conn.socketfd == -1){
                        std::cerr << "[ConnectionFail] (" << new_conn.ip_address << ':' << new_conn.port << "): " << strerror(errno) << '\n';
                    } else{

                        std::cerr << "[Connection] Establishing connection with (" << new_conn.ip_address << ':' << new_conn.port << ")\n";
                        try{
                            AddNewUserConnection(new_conn);
                        } catch(std::runtime_error& err){
                            std::cerr << err.what() << std::endl;
                            close(new_conn.socketfd);
                        }
                    }
                }
                else{ // just a regular client sending a message
                    msg_buffer.clear();
                    bool client_disconnect = false;
                    try{
                        ReceiveMessage(poll_obj.fd, msg_buffer.data());
                        if (msg_buffer.size() == 0){
                            break;
                        }

                    } catch (ReceiveMessageFail& err){
                        std::cerr << err.what() << std::endl;
                    } catch (ClientDisconnectError& err){
                        client_disconnect = true;
                    }

                    if (client_disconnect){
                        break; // since the iterators could have been invalidated after disconnecting the client
                    }
                }
            }
        }
    }
}

void ChatServer::Shutdown(){
    std::cerr << "[Shutdown] Shutting down the server..." << std::endl;
    for (const auto& [socketfd, user] : socket_to_user_){
        Disconnect(socketfd);
    }
    close(serv_socket_);

    std::cerr << "[Shutdown] Server is shut down!" << std::endl;
}

int main(int argc, char* argv[]){
    if (argc != 3){
        std::cerr << "[Usage] ./server <hostname> <port>" << std::endl;
        return 1;
    }
    
    std::unique_ptr<ChatServer> server;
    
    try{
        server = std::make_unique<ChatServer>(argv[1], argv[2]); // lazy initialization
    } catch(ServerInitializationFailed& err){
        std::cerr << err.what() << std::endl;
        return 1;
    }

    std::cout << "Initialized the server. Launching.." << std::endl;

    try{
        server->Launch();
    } catch(std::exception& err){
        std::cerr << err.what() << std::endl;
        return 1;
    }

}