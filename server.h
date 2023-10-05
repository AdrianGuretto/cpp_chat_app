#pragma once

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <poll.h>
#include <unistd.h>
#include <errno.h>

#include <signal.h>

#include <string.h>

#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "exceptions.h"
#include "domain.h"

#define BACKLOG 10
#define MESSAGE_MAX_SIZE 2048

/* Client-Server Communication Protocol
NOTE: every message has to be in the following format: <msg_len><message>

Establishing connection
1) Upon connecting to the server, a client sends a message containing its nickname (20 characters max)
2) Only after receiving client's nickname, the server adds the client to the active clients list

Message Transmittion
1) A client sends a message to the server
2) The server processes the message to see if it's a command.
    a) If the message is a command, then the server sends response to the client and/or notifies other connected users
    b) If it is a regular message, then the server itarates over connected sockets and sends them the message
*/

/* Server Working Scheme

1) To initializa the server, hostname and port should be supplied to its constructor (throws ServerInitializationFailed exception)
    1.1) hostname and port are checked for validity using getaddrinfo() syscall.
    1.2) a socket is created for the server.
    1.3) setsockopt() with SO_REUSEADDR is set for the socket.
    1.3) bind() is called on the server socket to the address from getaddrinfo() -> free addrinfo struct
2) Server starts with call to Launch() method and waits for incoming connections
    2.1) A server goes into while loop with AcceptConnection() method, which returns the accepted connection structure (socketfd is -1 and errno is set if an error occurs)
*/

class ChatServer final{
private: // --------- CLASS INTERNAL STRUCTURES ---------
public: // --------- CONSTRUCTORS/OPERATORS ---------

    // Main server constructor
    // @throw ServerInitializationFailed on any error
    explicit ChatServer(char* hostname, char* port, int addr_ver = AF_INET);

    explicit ChatServer(const ChatServer& other) = delete;
    explicit ChatServer(ChatServer&& other) = delete;
    ChatServer& operator=(const ChatServer& other) = delete;
    ChatServer& operator=(ChatServer&& other) = delete;

    ~ChatServer();

public: // --------- MAIN API ---------
    /**
     * Main server starting method.
     * @throw 
    */
    void Launch();

    /**
     * Main Server stopping method.
     * 
    */
    void Shutdown();
private: // --------- HELPER METHODS ---------

    /**
     * Transform an arbitrary message string to the protocol form: <len><msg>
     * @return A message in form: <len><msg>
    */
    std::string AssembleMessage(const std::string& message);
    std::string AssembleMessage(std::string&& message);

    /**
     * Transform an arbitrary message string to the protocol form: <len><msg> and append the sender name to the beginning of the message/
    */
    std::string AssembleClientMessage(int sender_socketfd, const std::string& message);
    std::string AssembleClientMessage(int sender_socetfd, std::string&& message);


    // Checks if the name does not contain any special characters (ASCII 33-126 are valid)
    bool IsValidName(const char* name);

    // Checks if the name is not taken already
    bool IsAvailableName(const char* name);

    /**
     * Internal sending function. Protects message's integrity and makes sure that all bytes have been sent.
     * @param socketfd socket of a client
     * @param buf pointer to a buffer to read data for sending from
     * @param len length of the data to send
     * @throws SendMessageFail
    */
    void SendAllBytes(int socketfd, const char* buf, size_t len);

    /** 
     * Send a message to an unaccepted yet connection.
     * @param socketfd socket file descriptor of a client
     * @param message message for the client
     * @throws SendMessageFail
    */
    void SendMessage(int socketfd, std::string&& message);

    /** 
     * Send a message to a client.
     * @param socketfd socket file descriptor of a client
     * @param message message for the client
     * @throws SendMessageFail
    */
    void SendMessageClient(int socketfd, const std::string& message);
    void SendMessageClient(int socketfd, std::string&& message);

    /**
     * Receive a message from a client.
     * @param socketfd a socket file descriptor of a client
     * @param buf a pointer to a writable buffer
     * @throw ReceiveMessageFail
    */
    void ReceiveMessage(int socketfd, char* buf);

    /**
     * Broadcast a message to every active client.
     * @throws BroadcastFail
    */
    void BroadcastMessage(int sender_socketfd, const std::string& message);
    void BroadcastMessage(int sender_socketfd, std::string&& message);

    /**
     * Parse the message to see if it is a command or a regular message, and handle them accordingly
     * @param sender_socketfd socket of a sender
     * @param message a message received from the client
    */
    void HandleMessage(int sender_socketfd, const std::string& message);

    /**
     * Accept a new connection on a listenning socket. If an error has occurred, socketfd with -1 is returned and errno is set.
     * @param addr pointer to the structure for holding connection information
     * @param addr_len pointer to the variable holding the size of the addr variable
     * @return Information about the new connection including new socketfd
    */
    NewConnectionInfo AcceptNewConnection(sockaddr* addr, socklen_t* addr_len);

    /**
     * Establish a connection to the server with the protocol:
     * 1) The server sends NICK_PROMPT and waits for the client's next message with its nickname
     * 2) A client sends the message (20 bytes max) with its nickname
     * 3) The server validates the name (no special chars, and the name is not taken). If the name is not valid, the server sends NICK_RETRY key message and repeats the speps above.
     * 4) If the name is valid, the server sends NICK_OK to the client and sends the welcoming message with active users list
     * 
     * @param conn_info Information about the newly connected client.
     * @throw EstablishConnectionFail
     * 
    */
    User EstablishConnection(const NewConnectionInfo& conn_info);

    /**
     * Main method for handling newly accepted connections.
     * @param conn_info Information about the newly connected client returned by AcceptNewConnection method
     * @throw EstablishConnectionFail, BroadcastFail
    */
    void AddNewUserConnection(const NewConnectionInfo& conn_info);


    /**
     * Disconnect a client from the server.
     * @param socketfd socket file descriptor of the client
    */
    void Disconnect(int socketfd);

private: // -------- FIELDS ---------
    const std::string hostname_, port_;
    int serv_socket_;

    std::unordered_set<std::string> active_users;

    std::unordered_map<int, User> socket_to_user_;
    std::vector<pollfd> poll_objects_;
};