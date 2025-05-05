#pragma once

#ifndef UNIPIPE_H
#define UNIPIPE_H

#include <unordered_map>
#include <iostream>
#include <string>
#include <stdexcept>
#include <memory>
#include <streambuf>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include "DescMan.hpp"

struct NamedPipeDescriptor {
    enum mode {
        server,
        client
    };

    std::string pipeName;
    mode pipeMode;

#ifdef _WIN32
    HANDLE pipeHandle;
#else
    int pipeFd;
#endif
};

struct PipeAllocator {
    NamedPipeDescriptor* operator()(const std::string& pipeName, NamedPipeDescriptor::mode pipeMode) {
#ifdef _WIN32
        HANDLE pipeHandle;
        if (pipeMode == NamedPipeDescriptor::server) {
            pipeHandle = CreateNamedPipeA(
                pipeName.c_str(),
                PIPE_ACCESS_DUPLEX,  // bi-directional communication
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                PIPE_UNLIMITED_INSTANCES,
                512,  // Output buffer size
                512,  // Input buffer size
                0,    // Default timeout
                NULL  // Default security attributes
            );
            if (pipeHandle == INVALID_HANDLE_VALUE) {
                throw std::runtime_error("Failed to create named pipe");
            }
            if (!ConnectNamedPipe(pipeHandle, NULL)) {
                throw std::runtime_error("Failed to connect to named pipe");
            }
        }
        else {
            BOOL bResult = WaitNamedPipeA(
                pipeName.c_str(),
                NMPWAIT_WAIT_FOREVER
            );
            if (!bResult) {
                throw std::runtime_error("Failed to wait for named pipe");
            }
            pipeHandle = CreateFileA(
                pipeName.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );
            if (pipeHandle == INVALID_HANDLE_VALUE) {
                throw std::runtime_error("Failed to connect to named pipe");
            }
        }
        NamedPipeDescriptor* pipeDescriptor = new NamedPipeDescriptor();
        pipeDescriptor->pipeHandle = pipeHandle;
        pipeDescriptor->pipeName = pipeName;
        pipeDescriptor->pipeMode = pipeMode;
        return pipeDescriptor;
#else
        int pipeFd;
        if (pipeMode == NamedPipeDescriptor::server) {
            if (mkfifo(pipeName.c_str(), 0666) == -1) {
                throw std::runtime_error("Failed to create named pipe");
            }
        }
        pipeFd = open(pipeName.c_str(), O_RDWR);
        if (pipeFd == -1) {
            throw std::runtime_error("Failed to connect to named pipe");
        }
        NamedPipeDescriptor* pipeDescriptor = new NamedPipeDescriptor();
        pipeDescriptor->pipeFd = pipeFd;
        pipeDescriptor->pipeName = pipeName;
        pipeDescriptor->pipeMode = pipeMode;
        return pipeDescriptor;
#endif
    }
};

struct PipeDeallocator {
    void operator()(NamedPipeDescriptor* pipeDescriptor) {
#ifdef _WIN32
        if (pipeDescriptor->pipeMode == NamedPipeDescriptor::server) {
            if (pipeDescriptor->pipeHandle != INVALID_HANDLE_VALUE) {
                DisconnectNamedPipe(pipeDescriptor->pipeHandle);
            }
        }
        if (pipeDescriptor->pipeHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(pipeDescriptor->pipeHandle);
        }
        delete pipeDescriptor;
#else
        if (pipeDescriptor->pipeFd != -1) {
            close(pipeDescriptor->pipeFd);
        }
        if (pipeDescriptor->pipeMode == NamedPipeDescriptor::server) {
            unlink(pipeDescriptor->pipeName.c_str());
        }
        delete pipeDescriptor;
#endif
    }
};

struct PipeDescriptorStringifier {
    std::string operator()(NamedPipeDescriptor* descriptor) {
        return std::to_string((uint64_t)descriptor);
    }
};

using PipeDescriptorManager = DescriptorManager<NamedPipeDescriptor*, PipeAllocator, PipeDeallocator, PipeDescriptorStringifier>;

class NamedPipe {
public:
    NamedPipe(const std::string& pipeName, NamedPipeDescriptor::mode pipeMode) :
        pipeDescriptor(PipeDescriptorManager::acquire(pipeName, pipeMode)) {
    }

    NamedPipe(NamedPipeDescriptor*& other) :
        pipeDescriptor(other) {
        PipeDescriptorManager::addRef(pipeDescriptor);
    }

    NamedPipe(const NamedPipe& other) :
        pipeDescriptor(other.pipeDescriptor) {
        PipeDescriptorManager::addRef(pipeDescriptor);
    }
    NamedPipe& operator=(const NamedPipe& other) {
        if (this != &other) {
            PipeDescriptorManager::addRef(other.pipeDescriptor);
            PipeDescriptorManager::removeRef(pipeDescriptor);
            pipeDescriptor = other.pipeDescriptor;
        }
        return *this;
    }

    ~NamedPipe() {
        PipeDescriptorManager::removeRef(pipeDescriptor);
    }

    int write(const void* buffer, const size_t length) {
#ifdef _WIN32
        DWORD bytesWritten;
// disablw warning C4267: 'argument': conversion from 'size_t' to 'DWORD', possible loss of data
#pragma warning(push)
#pragma warning(disable: 4267)
        if (!WriteFile(pipeDescriptor->pipeHandle, buffer, length, &bytesWritten, NULL)) {
#pragma warning(pop)
            throw std::runtime_error("Failed to write to named pipe");
        }
        FlushFileBuffers(pipeDescriptor->pipeHandle);
        return bytesWritten;
#else
        ssize_t bytesWritten = ::write(pipeDescriptor->pipeFd, buffer, length);
        if (bytesWritten == -1) {
            throw std::runtime_error("Failed to write to named pipe");
        }
        return bytesWritten;
#endif
    }

    int read(void* buffer, const size_t length) {
#ifdef _WIN32
        DWORD bytesRead;
// disablw warning C4267: 'argument': conversion from 'size_t' to 'DWORD', possible loss of data
#pragma warning(push)
#pragma warning(disable: 4267)
        if (!ReadFile(pipeDescriptor->pipeHandle, buffer, length, &bytesRead, NULL)) {
#pragma warning(pop)
            throw std::runtime_error("Failed to read from named pipe");
        }
        return bytesRead;
#else
        ssize_t bytesRead = ::read(pipeDescriptor->pipeFd, buffer, length);
        if (bytesRead == -1) {
            throw std::runtime_error("Failed to read from named pipe");
        }
        return bytesRead;
#endif
    }

private:
    NamedPipeDescriptor* pipeDescriptor;
};

class NamedPipeBuffer : public std::streambuf {
private:
    std::shared_ptr<NamedPipe> pipe;
    static const int bufferSize = 1024;
    char inputBuffer[bufferSize];
    char outputBuffer[bufferSize];

protected:
    // Read from socket into the input buffer
    int underflow() override {
        if (gptr() < egptr()) // If there are unread characters
            return traits_type::to_int_type(*gptr());

        int num_bytes = pipe->read(inputBuffer, bufferSize);
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
            int written = pipe->write(outputBuffer, num_bytes);
            if (written != num_bytes)
                return -1; // Error in sending

            pbump(-num_bytes); // Reset buffer
        }
        return 0;
    }

public:
    explicit NamedPipeBuffer(const std::shared_ptr<NamedPipe>& pipe) : pipe(pipe) {
        setg(inputBuffer, inputBuffer, inputBuffer);
        setp(outputBuffer, outputBuffer + bufferSize);
    }

    explicit NamedPipeBuffer() {
        setg(inputBuffer, inputBuffer, inputBuffer);
        setp(outputBuffer, outputBuffer + bufferSize);
    }

    ~NamedPipeBuffer() {
        sync(); // Flush remaining output buffer
    }

    void setPipe(const std::shared_ptr<NamedPipe>& newPipe) {
        if (pipe) {
            sync();
        }
        pipe = newPipe;
    }
};

class pipestream : public std::iostream {
private:
    NamedPipeBuffer pipeBuf;
    std::shared_ptr<NamedPipe> pipe;

public:
    enum mode {
        server,
        client
    };

    explicit pipestream() : std::iostream(&pipeBuf), pipeBuf(pipe) {}

    explicit pipestream(const std::string& pipeName, NamedPipeDescriptor::mode streamMode) : std::iostream(&pipeBuf) {
        open(pipeName, streamMode);
    }

    ~pipestream() {
        close();
    }

    void open(const std::string& pipeName, NamedPipeDescriptor::mode streamMode) {
        pipe = std::make_shared<NamedPipe>(pipeName, streamMode);
        pipeBuf.setPipe(pipe);
    }

    bool is_open() const {
        return pipe != nullptr;
    }

    void close() {
        pipe.reset();
        pipeBuf.setPipe(pipe);
    }
};

#endif // UNIPIPE_H
