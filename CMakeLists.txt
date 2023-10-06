cmake_minimum_required(VERSION 3.11)

project(ChatApp CXX)
set(CXX_STANDARD 17)

set(SERVER_SRCS_DIR "src/server")
set(CLIENT_SRCS_DIR "src/client")

set(CLIENT_FILES "${CLIENT_SRCS_DIR}/client.cpp" "${CLIENT_SRCS_DIR}/client.h" "${CLIENT_SRCS_DIR}/exceptions.h" "${CLIENT_SRCS_DIR}/domain.h")
set(SERVER_FILES "${SERVER_SRCS_DIR}/server.cpp" "${SERVER_SRCS_DIR}/server.h" "${SERVER_SRCS_DIR}/exceptions.h" "${SERVER_SRCS_DIR}/domain.h")

add_executable(client ${CLIENT_FILES})
add_executable(server ${SERVER_FILES})