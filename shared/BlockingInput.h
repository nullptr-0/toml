#pragma once

#ifdef EMSCRIPTEN
#include <iostream>
#include <streambuf>
#include <string>
#include <vector>
#include <cstring>

#include <emscripten/emscripten.h>

EM_JS(int, WaitForStdin, (), {
    let fs = require('fs');
    let inputBuffer = Buffer.alloc(1);
    fs.readSync(0, inputBuffer, 0, 1);
    return inputBuffer.toString().charCodeAt(0);
});

class BlockingStdinBuffer : public std::streambuf {
public:
    BlockingStdinBuffer() {
        setg(buffer.data(), buffer.data(), buffer.data()); // Initially empty
    }

protected:
    int_type underflow() override {
        // Fill the buffer with one line of input
        std::size_t len = 0;
        while (len < buffer.size() - 1) {
            int ch = WaitForStdin();
            if (ch == -1) {
                return traits_type::eof(); // EOF
            }

            char c = static_cast<char>(ch);
            buffer[len++] = c;

            if (c == '\n') break; // End of line
        }

        if (len == 0)
            return traits_type::eof();

        setg(buffer.data(), buffer.data(), buffer.data() + len);
        return traits_type::to_int_type(*gptr());
    }

private:
    static constexpr std::size_t bufsize = 1024;
    std::vector<char> buffer = std::vector<char>(bufsize);
};

class BlockingStdinStream : public std::istream {
public:
    BlockingStdinStream() : std::istream(&buf) {}
private:
    BlockingStdinBuffer buf;
};
#endif
