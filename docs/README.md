# A C++ chat client-server application
A simple TCP/IPv4 chat application written in C/C++, which purpose was to give me fundamental knowledge about UNIX socket programming as well as application design knowledge. Many of its concepts are derived from the [IRC Protocol](https://www.rfc-editor.org/rfc/rfc1459.html#page-13 "IRC Protocol Description"). 
In the next sections will be the outline of the project defining communication protocols, commands, and other abilities and limitations of the chat application. 

## ⚙️ Requirements

1. C++ 17 or higher
2. CMake 3.11 or higher
3. Linux-based OS
4. A firewall settings configured for allowing incoming connections (optional)

## 🔨 Installation

Currently, the project only supports Linux-Based OSes. In the future, I plan to add compatability with Windows.
To install the project, run these commands:
```
git clone https://github.com/AdrianGuretto/cpp_chat_app.git
cd cpp_chat_app && mkdir build
cd build && cmake ..
cmake --build .
```

After the installation is complete, you will have two executable files in your current directory: **server** and **client**, which you can run depending on the mode you want to launch

## 🚶‍♂️ Usage

To run the application, you need to have a currently running server. To launch the server, execute the command:  

```./server <hostname> <port>```  

Where `hostname` usually represents an IP address of the server, and `port` is, well, the port for the IP address.  

After the server has been launched, you can connect clients by running

```./client <server_hostname> <port>```

The arguments of which are quite self-explanatory.

## 🔛 Communication Protocol

The communication protocol consists of two parts: *establishing connection* and *in-server communication*.  
The client-server communication's keystone is **11-byte Key Signals**, which the client and the server exchange. *Key Signals* start with special character: '\07' (ASCII *BEL*); if a Key Signal accepts arguments, then the arguments follow the signal.  

*Server's Key Signals:*
```
NICK_PROMPT     :   Prompt the user to send the nickname
NICK_ACCEPT     :   Tell a client that the nickname has been accepted
NICK_STAKEN     :   Tell a client that the nickname is not valid (taken by someone else on the server)
NICK_INVALD     :   Tell a client that the nickname is not valid (contains special characters or spaces)
```

*Client's Key Signals*  
If a command contains arguments, then each argument is separated by ASCII character start-of-text (002)
```
NICK_NEWREQ                     :     Send the initial nickname (CONN_ESTABLISHING time only)
ACT_NICKCNG<new_name>           :     Tell the server to change the nickname to a nickname
ACT_LSUSERS                     :     Inquire the server for active users (output format: "<connection_number>. <username> (<user_address>)")
ACT_PMSGUSR<username><message>  :     Send a private message to a user with <username>.
```
____
### Message Format

Each message, either from a client or the server, adheres to following format:

```<MESSAGE_LENGTH><MESSAGE>```

**MESSAGE_LENGTH** is derived from the assembled message (including the *Key Segnal's denoting char (ASCII 07)*)

___
### Establishing Connection


--------- **CLIENT CONNECTS TO THE SERVER** ---------

1) A client connects to the server and waits for the server Key Signal `NICK_PROMPT` to prompt for username.  
_BEGIN LOOP_
2) Upon receiving the signal, user enters its username to the input.
3) Client sends its chosen username to the server prepending message with `NICK_NEWREQ` Key Signal.
4) Server receives the name, analyzes it, and send of the *NICK_XXX* Key Signals
5) If the signal is not `NICK_ACCEPT`, then we continue the loop.  
_END LOOP_
6) Server adds the client to the list of active connections and notifies everyone on the server about the newly connected user.

--------- **CONNECTION IS ESTABLISHED** ---------

___
### In-Server Communication

**CLIENT:**  
Client runs displaying and accepting-input functions on two separate, non-blocking threads:  
#### Receiving
```
1. A user repeatedly tries to read from the server socket.
2. If received bytes are one and more, then it means we have received data.
3. A message packet is disassembled by repeatedly reading chunks of data from the server socket:
    1. Read first 4 bytes of the packet (4 bytes = 4 digits in 1024; ) to get the length of the message.
    2. Read the number of bytes from the received message length number.
    3. Check if the message is a Key Signal: if yes, then we pass it to Key Signal handling function; if not, then we display the message on the screen  
```
#### Sending
```
1. A user enters a message to the input reader and the reader checks if the message is less than 1024 bytes (MESSAGE_LENGTH_LIMIT)
2. If the message is more than 1024 bytes, then _**MSG_LEN_LIMIT_EXCEEDED**_ error flag.
3. The message is processed by message-assembling functions
    a) If the message starts with **/**, then it is a command and the command is transformed to Key Signal message (with '\07' at the beginning)
    b) Else it is a regular message.
4. Add the length of the processed message at the end -> the message is assembled
5. The assembled message is sent to the server socket.
```
**SERVER:**  
Server iterates over the active `pollfd` objects using [poll()](https://man7.org/linux/man-pages/man2/poll.2.html) system call and checks if a socket is ready to be read from. We only need to check for readable sockets, because, if there is something to read, then we only have to call broadcasting function that will perform its own iteration over the list of sockets and will check for writeable ones.
#### Receiving
```
1. A server runs through the list of active clients (poll() objects) to see available for reading sockets
2. If a socket is availbale for reading, check if:
    1. The available socket is the server socket, then it is a new connection -> accept new connection -> Establish Connection
    2. Usual socket.
3. Disassemble the message packet by repeatedly reading chunks of data from the socket:
    1. Receive the first 4 bytes of the packet to get the message length;
    2. Read the received message length.
    3. Check if the message is a Key Signal: 
        a) if it starts with '\07' character, then handle the Signal;
        b) If it is a regular message, broadcast it to every active connection.
```
#### Sending
```
1. A message is assembled based on its type:
    1. Key Signal Message: assembled_string.length() + assembled_string('\07' + KEY_SIGNAL)
    2. Regular Message: assembled_string.length() + assembled_string(MESSAGE)
2. Send the message to the client. On error: Disconnect the client from the server.
```


## 🆕 Future Updates
1. Add end-to-end encryption (AES—RSA—Diffie–Hellman algorithm)
2. Add GUI interface (QT-based)
3. Add support for channels.
4. Add support for creating permanent user accounts.
5. Add support for IPv6 addresses.