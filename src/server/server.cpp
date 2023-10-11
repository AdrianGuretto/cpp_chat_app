#include "server.h"

Server::Server(char* hostname, char* port) : hostname_(hostname), port_(port) {
    std::cerr << MakeColorfulText("[ServInit] Configuring the server..."s, Color::Yellow) << '\n';

    addrinfo hints, *res_addr;

    // We want a IP/TCP socket for sending and receiving messages
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // Get a possible address structure for a new socket
    int getaddrinfo_status_code = getaddrinfo(hostname_.data(), port_.data(), &hints, &res_addr);
    if (getaddrinfo_status_code != 0){
        throw std::runtime_error("getaddrinfo(): "s + std::string(gai_strerror(getaddrinfo_status_code)));
    }

    // Create server socket
    server_socket_ = socket(res_addr->ai_family, res_addr->ai_socktype, res_addr->ai_protocol);
    if (server_socket_ == -1){
        throw std::runtime_error("socket(): "s + std::string(strerror(errno)));
    }

    // Bind the server socket to an available address
    if (bind(server_socket_, res_addr->ai_addr, res_addr->ai_addrlen) == -1){
        throw std::runtime_error("bind(): "s + std::string(strerror(errno)));
    }
    freeaddrinfo(res_addr);

    // Allow launching the server right after a shutdown.
    SetSocketOption(server_socket_, SO_REUSEADDR);

    std::cerr << MakeColorfulText("[ServInit] Successfully configured the server!"s, Color::Green) << '\n';
}

int Server::ProcessMessage(int sender_socketfd, char* readable_buffer, std::vector<DisconnectedClient>& disconnected_storage){
    std::cerr << "ProcessMessage() call" << std::endl;

    std::string msg_str(readable_buffer);
    if (msg_str.size() == 0){ // TO DO: Make sure that no message is empty
        return 0;
    }
    ConnectionInfo conn_inf = GetConnectionInfoFromSocket(sender_socketfd);
    if (msg_str[0] == '\07'){
        std::string command_str(msg_str.substr(1));
        const auto send_msg_with_errorchecking = [&](std::string&& message){
            if (SendMessage(sender_socketfd, std::move(message)) == -1){
                if (sock_to_user_.count(sender_socketfd)){ // check if this is a connected client
                    disconnected_storage.push_back(DisconnectedClient{.socket_fd = sender_socketfd, .disconnect_reason = "message delivery failed: "s + std::string(strerror(errno))});
                } else{ // not yet connected client
                    return -1;
                }
            }
            return 0;
        };
        switch (StringToClientKeySignal(command_str.substr(0, 11))){ // 11 = key signal bytes length
            case ClientKeySignal::UNKNOWN:
            {
                send_msg_with_errorchecking(std::string("Unknown command: "s + command_str.substr(0, 11)));
                break;
            }
            case ClientKeySignal::NICK_NEWREQ: // Client sending its initial nickname
            {
                std::string nickname(command_str.substr(11));
                NicknameAction nick_action = __ValidateNickname__(nickname.data());
                if (nick_action == NicknameAction::NICK_ACCEPT){
                    send_msg_with_errorchecking("\07NICK_ACCEPT"s);
                    
                    User new_user{.nickname = nickname, .ip_address = conn_inf.ip_address, .port = std::to_string(conn_inf.port)};

                    pollfd new_user_pollobj;
                    new_user_pollobj.events = POLLIN | POLLOUT;
                    new_user_pollobj.fd = sender_socketfd;
                    
                    poll_objects_.push_back(std::move(new_user_pollobj));
                    sock_to_user_[sender_socketfd] = std::move(new_user);
                    taken_nicknames_.insert(nickname);
                    BroadcastMessage(MakeColorfulText("[Connection] "s + nickname + " "s + conn_inf.ToString() + " has connected."s, Color::Green));
                    return send_msg_with_errorchecking(std::string("Welcome to the server! Currently active users: "s + std::to_string(sock_to_user_.size())));
                } else{
                    return send_msg_with_errorchecking(std::string(nickaction_to_keysig_string.at(nick_action)));
                }
                break;
            }
            case ClientKeySignal::ACT_PMSGUSR: // Client wants to send a Private Message to another one
            {
                int receipient_name_end_pos = command_str.find_first_of('\02');
                break; // DO THIS
            }
            default:
                break;
        }
    }
    else{
        if (sock_to_user_.count(sender_socketfd)){ // if the message is from connected client
            std::string sender_name = sock_to_user_.at(sender_socketfd).nickname;
            std::string final_msg;
            final_msg.append("["s).append(std::move(sender_name)).append("] "s).append(std::move(msg_str));
            BroadcastMessage(std::move(final_msg));
        }
        else{ // it is a message from an unconnected client -> protocol violation (possible DDOS)
            std::cerr << MakeColorfulText("client ("s + conn_inf.ip_address + ":"s + std::to_string(conn_inf.port) + ") failed to connect: message protocol violation. (msg: "s + std::string(readable_buffer) + ")."s, Color::Pink) << '\n';
            close(sender_socketfd); 
        }
    }
    return 0;
}

void Server::EstablishConnection(std::vector<pollfd>& pending_connections_vec, sockaddr_storage* addr_storage, socklen_t* addr_len_ptr){
    int new_conn_socketfd = AcceptNewConnection(addr_storage, addr_len_ptr);
    ConnectionInfo new_conn_info = GetConnectionInfoFromSocket(new_conn_socketfd);
    std::cerr << "[Connection] "s << new_conn_info.ToString() << " is trying to connect.\n"s;

    if (new_conn_socketfd == -1){
        DeletePendingConnection(new_conn_info, new_conn_socketfd, strerror(errno));
    }
    
    // Begin the handshake
    if (SendMessage(new_conn_socketfd, "\07NICK_PROMPT") != 0){
        DeletePendingConnection(new_conn_info, new_conn_socketfd, strerror(errno));
    }

    pollfd conn_pollfd;
    conn_pollfd.fd = new_conn_socketfd;
    conn_pollfd.events = POLLIN | POLLOUT; // get notified when the socket is readable or writable
    pending_connections_vec.push_back(std::move(conn_pollfd));
}

void Server::HandlePendingConnections(std::vector<pollfd>& pending_connections_vec, char* read_buffer){
    std::cerr << "HandlePendingConnections call" << std::endl;
    int poll_count = poll(pending_connections_vec.data(), pending_connections_vec.size(), 200); // 200 miliseconds wait.
    if (poll_count == 0){
        return;
    }
    else if (poll_count == -1){
        throw std::runtime_error("Failed to extract pending connections: poll(): "s + std::string(strerror(errno)));
    }
    
    static std::vector<DisconnectedClient> failed_clients;
    failed_clients.reserve(sock_to_user_.size());

    static std::vector<pollfd> new_connected_clients;
    new_connected_clients.reserve(sock_to_user_.size());
    for (size_t i = 0; i < pending_connections_vec.size(); ++i){
        pollfd& poll_obj = pending_connections_vec[i];
        if (poll_obj.revents & POLLIN){ // a client is sending us something
            if (ReceiveMessage(poll_obj.fd, read_buffer) == 0){
                failed_clients.push_back(DisconnectedClient{.socket_fd = poll_obj.fd, .disconnect_reason = "client disconnect."s});
            }
            if (ProcessMessage(poll_obj.fd, read_buffer, failed_clients) == 0){ // if connection has been successful
                new_connected_clients.push_back(poll_obj);
            } else{
                failed_clients.push_back(DisconnectedClient{.socket_fd = poll_obj.fd, .disconnect_reason = "client failed to connect: "s + std::string(strerror(errno))});
            }
        }
    }

    // Removing successfully connected clients from pending connections list
    for (pollfd& poll_obj : new_connected_clients){
        pending_connections_vec.erase(std::find_if(pending_connections_vec.begin(), pending_connections_vec.end(), [&poll_obj](const pollfd& poll_obj2){
            return poll_obj.fd == poll_obj2.fd;
        }));
    }

    DisconnectClient(failed_clients);
    failed_clients.clear();
    new_connected_clients.clear();
}

int Server::AcceptNewConnection(sockaddr_storage* addr_storage, socklen_t* addr_len_ptr){
    int new_conn_socketfd = accept(server_socket_, reinterpret_cast<sockaddr*>(addr_storage), addr_len_ptr);
    if (new_conn_socketfd == -1){
        throw std::runtime_error("Failed to accept new connection: accept(): "s + std::string(strerror(errno)));
    }
    return new_conn_socketfd;
}

void Server::Start(){
    std::cerr << MakeColorfulText("[ServStart] Starting the server..."s, Color::Yellow) << '\n';

    signal(SIGINT, InterruptHandler);

    __SetUpListenner__();

    std::cerr << MakeColorfulText("[ServStart] Server is up! (accepting connections on "s + hostname_ + ":"s + port_ + ")"s, Color::Green) << '\n';

    char read_buffer[1028]; // +4 bytes for message header (msg_len)
    memset(&read_buffer, 0, sizeof(read_buffer));
    std::cerr << "Server socketfd: "s << server_socket_ << std::endl;
    sockaddr_storage new_connection_addr;
    socklen_t new_conn_addrlen;

    int poll_count;
    const auto check_poll_count_error = [&poll_count](){
        if (poll_count == -1){
            std::string error_msg("Listen for connections failed: poll(): "s + std::string(strerror(errno)));
            throw std::runtime_error(MakeColorfulText(std::move(error_msg), Color::Red));
        }
    };
    std::vector<DisconnectedClient> disconnecting_clients; // stores clients who want to disconnect (invalidation of iterators in the for-range)
    std::vector<pollfd> pending_connections;
    disconnecting_clients.reserve(30);
    pending_connections.reserve(30);
    while (EXIT_SIGNAL == 0){
        DisconnectClient(disconnecting_clients);

        // check pending connections
        poll_count = poll(pending_connections.data(), pending_connections.size(), 200);
        check_poll_count_error();

        HandlePendingConnections(pending_connections, read_buffer);

        // check for regular data
        poll_count = poll(poll_objects_.data(), poll_objects_.size(), 200);
        check_poll_count_error();

        // run through active connections to see if there is data to read
        for (const pollfd& poll_obj : poll_objects_){
            // check if the socket is ready to be read
            if (poll_obj.revents & POLLIN){ 
                if (poll_obj.fd == server_socket_){ // serv_socket ready-to-be-read = new connection data
                    EstablishConnection(pending_connections, &new_connection_addr, &new_conn_addrlen);
                }
                else{ // regular client's message
                    ProcessMessage(poll_obj.fd, read_buffer, disconnecting_clients);
                }
            }
        }
    }

    ShutDown();
}

void Server::ShutDown() noexcept{
    std::cerr << MakeColorfulText("[ServerShutdown] Shutting down..."s, Color::Pink) << '\n';
    for (const auto& [socketfd, user] : sock_to_user_){
        close(socketfd);
    }
    close(server_socket_);
    std::cerr << MakeColorfulText("[ServerShutdown] Bye!"s, Color::Pink) << '\n';
}

Server::~Server(){
    ShutDown();
}

void Server::__SetUpListenner__(){
    // Set up the listenner socket
    if (listen(server_socket_, BACKLOG) == -1){
        throw std::runtime_error("listen(): "s + std::string(strerror(errno)));
    }

    // Add the initial poll object - server socket
    pollfd listenner_pollobj;
    listenner_pollobj.fd = server_socket_;
    listenner_pollobj.events = POLLIN;

    poll_objects_.push_back(std::move(listenner_pollobj));
}

NicknameAction Server::__ValidateNickname__(const char* client_nickname_change_message) noexcept{
    // Extract the nickname from the message
    int nickname_length = strlen(client_nickname_change_message); // 11 bytes for key signal length + 1 byte for key signal char 
    int char_ascii_code;
    for (int i = 12; i < nickname_length; ++i){
        char_ascii_code = static_cast<int>(client_nickname_change_message[i]);
        if (char_ascii_code < 32 || char_ascii_code > 126){ // Acceptable ASCII chars: 32-126
            return NicknameAction::NICK_INVALD;
        }
    }

    if (taken_nicknames_.count(std::string(client_nickname_change_message + 12))){
        return NicknameAction::NICK_STAKEN;
    }
    return NicknameAction::NICK_ACCEPT;
}

void Server::BroadcastMessage(std::string&& message){
    std::vector<DisconnectedClient> errored_clients;
    errored_clients.reserve(poll_objects_.size());
    for (const pollfd& poll_obj_ : poll_objects_){
        if (poll_obj_.revents & POLLOUT){
            if (SendMessage(poll_obj_.fd, message) == -1){
                errored_clients.push_back(DisconnectedClient{.socket_fd = poll_obj_.fd, .disconnect_reason = "message delivery failed: "s + std::string(strerror(errno))});
            }
        }
    }
    DisconnectClient(std::move(errored_clients));
}

void Server::DisconnectClient(DisconnectedClient&& disconn_info) noexcept{
    if (sock_to_user_.count(disconn_info.socket_fd)){ // if the client is connected.
        User disc_client = sock_to_user_.at(disconn_info.socket_fd);

        sock_to_user_.erase(disconn_info.socket_fd);
        taken_nicknames_.erase(disc_client.nickname);
        poll_objects_.erase(std::remove_if(poll_objects_.begin(), poll_objects_.end(), [&](pollfd& poll_obj){
            return poll_obj.fd = disconn_info.socket_fd;
        }), poll_objects_.end());

        BroadcastMessage(std::string(disc_client.nickname + " ("s + disc_client.ip_address + ":"s + disc_client.port + ") has been disconnected, reason: "s + std::move(disconn_info.disconnect_reason)));
    } else{ // if the client hasn't established the connection
        ConnectionInfo conn_inf = GetConnectionInfoFromSocket(disconn_info.socket_fd);
        close(disconn_info.socket_fd);
        std::cerr << MakeColorfulText("[ConnectionFail] Unconnected client "s + conn_inf.ToString() + " has been disconnected: "s + disconn_info.disconnect_reason, Color::Red) << '\n'; // don't notify other clients about failed connections.
    }
}
void Server::DisconnectClient(const DisconnectedClient& disconn_info) noexcept{
    std::cerr << "DisconnectClient() socketfd: "s << disconn_info.socket_fd << std::endl;
    if (sock_to_user_.count(disconn_info.socket_fd)){ // if the client is connected.
        User disc_client = sock_to_user_.at(disconn_info.socket_fd);

        sock_to_user_.erase(disconn_info.socket_fd);
        taken_nicknames_.erase(disc_client.nickname);
        poll_objects_.erase(std::remove_if(poll_objects_.begin(), poll_objects_.end(), [&](pollfd& poll_obj){
            return poll_obj.fd = disconn_info.socket_fd;
        }), poll_objects_.end());

        BroadcastMessage(std::string(disc_client.nickname + " ("s + disc_client.ip_address + ":"s + disc_client.port + ") has been disconnected, reason: "s + std::move(disconn_info.disconnect_reason)));
    } else{ // if the client hasn't established the connection
        ConnectionInfo conn_inf = GetConnectionInfoFromSocket(disconn_info.socket_fd);
        close(disconn_info.socket_fd);
        std::cerr << MakeColorfulText("[ConnectionFail] Unconnected client "s + conn_inf.ToString() + " has been disconnected: "s + disconn_info.disconnect_reason, Color::Red) << '\n'; // don't notify other clients about failed connections.
    }
}
void Server::DisconnectClient(std::vector<DisconnectedClient>&& clients_to_disconnect) noexcept{
    for (DisconnectedClient& client : clients_to_disconnect){
        DisconnectClient(std::move(client));
    }
}
void Server::DisconnectClient(std::vector<DisconnectedClient>& clients_to_disconnect) noexcept{
    for (DisconnectedClient& client : clients_to_disconnect){
        DisconnectClient(std::move(client));
    }
    clients_to_disconnect.clear();
}


int main(int argc, char* argv[]){
    if (argc != 3){
        std::cerr << "[Usage] ./server <hostname> <port>"s << std::endl;
        return 1;
    }

    std::unique_ptr<Server> p_server;
    try{
        p_server = std::make_unique<Server>(argv[1], argv[2]);
    } catch(std::runtime_error& err){
        std::cerr << MakeColorfulText("[ServerInitFail] "s + std::string(err.what()), Color::Red) << std::endl;
        return 1;
    }

    try{
        p_server->Start();
    } catch(std::runtime_error& err){
        std::cerr << MakeColorfulText("[ServerFatalError] "s + std::string(err.what()), Color::Red) << std::endl;
        return 1;
    }
    
}