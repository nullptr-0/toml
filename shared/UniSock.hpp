#pragma once

#ifndef UNISOCK_H
#define UNISOCK_H

#include <unordered_map>
#include <string>
#include <iostream>
#include <streambuf>
#include <stdexcept>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include "DescMan.hpp"

struct SocketAllocator {
    int operator()() {
        int socketDescriptor = socket(AF_INET, SOCK_STREAM, 0);
        if (socketDescriptor < 0) {
            throw std::runtime_error("Failed to create socket");
        }
        return socketDescriptor;
    }
};

struct SocketDeallocator {
    void operator()(int socketDescriptor) {
#ifdef _WIN32
        closesocket(socketDescriptor);
#else
        close(socketDescriptor);
#endif
    }
};

using SocketDescriptorManager = DescriptorManager<int, SocketAllocator, SocketDeallocator>;

class RawSocket {
private:
    int socketDescriptor;
#ifdef _WIN32
    static bool wsaInitialized;

    void CheckAndPerformWSAInit() {
        if (!wsaInitialized) {
            WSADATA data;
            if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
                throw std::runtime_error("Failed to initialize Winsock");
            }
        }
    }

    void CheckAndPerformWSACleanup() {
        if (wsaInitialized) {
            WSACleanup();
        }
    }
#endif

public:
    RawSocket() {
#ifdef _WIN32
        CheckAndPerformWSAInit();
#endif
        socketDescriptor = SocketDescriptorManager::acquire();
    }

    RawSocket(const int& other) : socketDescriptor(other) {
#ifdef _WIN32
        CheckAndPerformWSAInit();
#endif
        SocketDescriptorManager::addRef(socketDescriptor);
    }

    RawSocket(const RawSocket& other) : socketDescriptor(other.socketDescriptor) {
#ifdef _WIN32
        CheckAndPerformWSAInit();
#endif
        SocketDescriptorManager::addRef(socketDescriptor);
    }

    ~RawSocket() {
        SocketDescriptorManager::removeRef(socketDescriptor);
#ifdef _WIN32
        CheckAndPerformWSACleanup();
#endif
    }

    // Assignment Operator: Handle self-assignment and reference counting
    RawSocket& operator=(const RawSocket& other) {
        if (this != &other) {
            SocketDescriptorManager::addRef(other.socketDescriptor);
            SocketDescriptorManager::removeRef(socketDescriptor);
            socketDescriptor = other.socketDescriptor;
        }
        return *this;
    }

    // Send raw data
    int send(const void* data, const size_t length) {
        return ::send(socketDescriptor, static_cast<const char*>(data), static_cast<int>(length), 0);
    }

    // Receive raw data
    int receive(void* buffer, const size_t bufferSize) {
        return ::recv(socketDescriptor, static_cast<char*>(buffer), static_cast<int>(bufferSize), 0);
    }

    // Listen for incoming connections
    void listen(const int port, const std::string host = "") {
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = host.empty() ? htonl(INADDR_ANY) : inet_addr(host.c_str());
        serverAddr.sin_port = htons(port);

        if (bind(socketDescriptor, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
            throw std::runtime_error("Failed to bind");
        }

        if (::listen(socketDescriptor, SOMAXCONN) < 0) {
            throw std::runtime_error("Failed to listen");
        }

        return;
    }

    // Accept an incoming connection
    RawSocket accept() {
        sockaddr_in clientAddr{};
        socklen_t clientAddrLen = sizeof(clientAddr);

        int clientSocketDescriptor = ::accept(socketDescriptor, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
        if (clientSocketDescriptor < 0) {
            throw std::runtime_error("Failed to accept connection");
        }

        return RawSocket(clientSocketDescriptor);
    }

    // Connect to a remote server
    void connect(const std::string& host, const int port) {
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = inet_addr(host.c_str());
        serverAddr.sin_port = htons(port);

        if (::connect(socketDescriptor, reinterpret_cast<const sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
            throw std::runtime_error("Failed to connect to server");
        }

        return;
    }
};

#ifdef _WIN32
bool RawSocket::wsaInitialized = false;
#endif

class SocketBuffer : public std::streambuf {
private:
    std::shared_ptr<RawSocket> socket;
    static const int bufferSize = 1024;
    char inputBuffer[bufferSize];
    char outputBuffer[bufferSize];

protected:
    // Read from socket into the input buffer
    int underflow() override {
        if (gptr() < egptr()) // If there are unread characters
            return traits_type::to_int_type(*gptr());

        int num_bytes = socket->receive(inputBuffer, bufferSize);
        if (num_bytes <= 0) // Check for errors or disconnect
            return traits_type::eof();

        setg(inputBuffer, inputBuffer, inputBuffer + num_bytes);
        return traits_type::to_int_type(*gptr());
    }

    // Write from output buffer to socket
    int overflow(int c) override {
        if (c != traits_type::eof()) {
            *pptr() = c;
            pbump(1);
        }
        return sync() == 0 ? c : traits_type::eof();
    }

    // Send data in the output buffer to the socket
    int sync() override {
        int num_bytes = int(pptr() - pbase());
        if (num_bytes > 0) {
            int sent = socket->send(outputBuffer, num_bytes);
            if (sent != num_bytes)
                return -1; // Error in sending

            pbump(-num_bytes); // Reset buffer
        }
        return 0;
    }

public:
    explicit SocketBuffer(const std::shared_ptr<RawSocket>& socket) : socket(socket) {
        setg(inputBuffer, inputBuffer, inputBuffer);
        setp(outputBuffer, outputBuffer + bufferSize);
    }

    explicit SocketBuffer() {
        setg(inputBuffer, inputBuffer, inputBuffer);
        setp(outputBuffer, outputBuffer + bufferSize);
    }

    ~SocketBuffer() {
        sync(); // Flush remaining output buffer
    }

    void setSocket(const std::shared_ptr<RawSocket>& newSocket) {
        if (socket) {
            sync();
        }
        socket = newSocket;
    }
};

using HostPortPair = std::pair<std::string, int>;

struct HostPortPairHasher {
    size_t operator()(const HostPortPair& pair) const {
        return
            std::hash<std::string>{}(pair.first) ^
            std::hash<int>{}(pair.second);
    }
};

class socketstream : public std::iostream {
private:
    SocketBuffer sockBuf;
    std::shared_ptr<RawSocket> socket;
    static std::unordered_map<HostPortPair, RawSocket, HostPortPairHasher> serverListen;

public:
    enum mode {
        server,
        client
    };

    explicit socketstream() : std::iostream(&sockBuf), sockBuf(socket) {}

    explicit socketstream(const std::string host, const int port, socketstream::mode streamMode) : std::iostream(&sockBuf) {
        open(host, port, streamMode);
    }

    ~socketstream() {
        close();
        serverListen.clear();
    }

    void open(const std::string host, const int port, socketstream::mode streamMode) {
        if (streamMode == mode::server && serverListen.find({ host, port }) != serverListen.end()) {
            socket = std::make_shared<RawSocket>(serverListen[{host, port}]);
        }
        else {
            socket = std::make_shared<RawSocket>();

            if (streamMode == mode::client) {
                socket->connect(host, port);
            }
            else {
                socket->listen(port, host);
                serverListen[{host, port}] = *socket;
                *socket = socket->accept();
            }
        }

        sockBuf.setSocket(socket);
    }

    bool is_open() const {
        return socket != nullptr;
    }

    void close() {
        socket.reset();
        sockBuf.setSocket(socket);
    }
};

std::unordered_map<HostPortPair, RawSocket, HostPortPairHasher> socketstream::serverListen;

// Usage Example:
// 
// #include <iostream>
// #include "unisock.hpp"
// 
// /* client side */
// int main() {
//     try {
//         socketstream client("127.0.0.1", 8080, socketstream::client); //  Connect to server on port 8080
// 
//         client << "Hello, server!" << std::endl;
// 
//         std::string response;
//         std::getline(client, response);
//         std::cout << "Server says: " << response << std::endl;
// 
//         client.close();
//     }
//     catch (const std::exception& e) {
//         std::cerr << "Error: " << e.what() << std::endl;
//     }
// 
//     return 0;
// }
// 
// /* server side */
// int main() {
//     try {
//         socketstream server("", 8080, socketstream::server); //  Listen on port 8080
// 
//         std::string client_message;
//         std::getline(server, client_message);
//         std::cout << "Received from client: " << client_message << std::endl;
// 
//         server << "Hello, client!" << std::endl;
// 
//         server.close();
//     }
//     catch (const std::exception& e) {
//         std::cerr << "Error: " << e.what() << std::endl;
//     }
// 
//     return 0;
// }

#endif // UNISOCK_H
