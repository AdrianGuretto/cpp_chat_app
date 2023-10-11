#pragma once

#include "../../lib/networking_ops.h"
#include "../../lib/color.h"

#include <stdexcept>

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <algorithm>
#include <signal.h>

#include "domain.h"

#define BACKLOG 10 // Max number of pending connections to the server
#define MESSAGE_MAX_LENGTH 1024;
#define CONNECTIONS_LIMIT 30;

int EXIT_SIGNAL = 0;
static void InterruptHandler(int signal_num){
    EXIT_SIGNAL = 1;
}

// TO DO: Finish the algorithm for accepting new connections
// TO DO: Switch from exceptions to return values.

class Server{
public:
    explicit Server(char* hostname, char* port);

    explicit Server(const Server& other) = delete;
    Server& operator=(const Server& other) = delete;

    ~Server();

public:
    /**
     * Start the server.
     * @throws std::runtime_error if an error has occurred.
    */
    void Start();

    /**
     * ShutDown the server and close all active connections.
    */
    void ShutDown() noexcept;

private: // --------- client actions ---------

    void BroadcastMessage(std::string&& message);

    /**
     * @param disconn_info structure with socket and disconnection reason for a client
    */
    void DisconnectClient(DisconnectedClient&& disconn_info) noexcept;
    void DisconnectClient(const DisconnectedClient& disconn_info) noexcept;
    void DisconnectClient(std::vector<DisconnectedClient>&& clients_to_disconnect) noexcept;
    void DisconnectClient(std::vector<DisconnectedClient>& clients_to_disconnect) noexcept;

    /**
     * Check if the message is a command or a regular text: if a regular message - broadcast to everyone, if a command - send a response to the client
     * @param sender_socketfd client's socket
     * @param readable_buffer a buffer where the message is stored
     * @param disconnected_storage a vector for storing disconnecting clients
     * @return -1 on error with a pending connection, 0 on everything else
    */
    int ProcessMessage(int sender_socketfd, char* readable_buffer, std::vector<DisconnectedClient>& disconnected_storage);

private: // --------- connection-handling functions ---------
    /**
     * Enable server socket to listen for incoming connections
     * @throw std::runtime_error on listen() -1 return
    */
    void __SetUpListenner__();

    /**
     * EstablishConnection's internal-use method: Create a new socket from incoming connection.
     * @param addr_storage structure for holding incoming connection's address
     * @param addr_len_ptr pointer to the length of addr_storage structure
     * @throw std::runtime_error on accept() -1 return <--- WORK ON THE EXCEPTION HANDLING
    */
    [[nodiscard]] int AcceptNewConnection(sockaddr_storage* addr_storage, socklen_t* addr_len_ptr);

    static void DeletePendingConnection(ConnectionInfo& conn_info, int socket_fd, char* fail_reason) noexcept{
        // close socket and print the fail text
        close(socket_fd);
        std::string err_msg("[ConnectionFail] "s);
        err_msg.reserve(100);
        err_msg.append(conn_info.ToString()).append(" has failed to connect: "s).append(std::string(fail_reason));
        std::cerr << MakeColorfulText(std::move(err_msg), Color::Red) << '\n';
    }

    /**
     * Add an incoming connection to the list of pending connections and perform the connection protocol.
     * @param pending_connections_vec vector to store the pending connection info
     * @param addr_storage a structure for holding info about a new connection.
     * @param addr_len_ptr a pointer to a variable with length of the incoming connection address
     * 
    */
    void EstablishConnection(std::vector<pollfd>& pending_connections_vec, sockaddr_storage* addr_storage, socklen_t* addr_len_ptr);

    /**
     * Handle clients that are waiting for their connection to the server to be established.
     * @param pending_connection_vec vector of pending connections with pollfd structs
     * @param read_buffer a pointer to a writable buffer.
     * @throw std::runtime_error on poll() -1 return or from ReceiveMessage or SendMessage methods.
    */
    void HandlePendingConnections(std::vector<pollfd>& pending_connections_vec, char* read_buffer);

    /**
     * Give response to client's nickname change.
     * @param client_nickname_change_message a raw message string yielded from recv() funciton
     * @return One of NicknameAction flags
    */
    NicknameAction __ValidateNickname__(const char* client_nickname_change_message) noexcept;


private:
    const std::string hostname_, port_;
    int server_socket_;

    
    std::unordered_set<std::string> taken_nicknames_;
    std::unordered_map<int, User> sock_to_user_;
    std::vector<pollfd> poll_objects_;
    
};
