#pragma once

#ifndef LOG_H
#define LOG_H

#include <iostream>
#include <fstream>
#include <string>
#include <ctime>

namespace Log
{
    enum class LogOutput {
        Console,
        File
    };

    enum class LogType {
        Info,
        Warning,
        Error
    };

#ifndef DEF_GLOBAL
    extern std::string getCurrentDate();
#else
    std::string getCurrentDate() {
        std::time_t now = std::time(nullptr);
        char buf[80];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::localtime(&now));
        return buf;
    }
#endif

#ifndef DEF_GLOBAL
    extern std::string getCurrentTime();
#else
    std::string getCurrentTime() {
        std::time_t now = std::time(nullptr);
        char buf[80];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        return buf;
    }
#endif

#ifndef DEF_GLOBAL
    extern std::string logTypeToString(LogType type);
#else
    std::string logTypeToString(LogType type) {
        switch (type) {
        case LogType::Info:
            return "INFO";
        case LogType::Warning:
            return "WARNING";
        case LogType::Error:
            return "ERROR";
        default:
            return "UNKNOWN";
        }
    }
#endif

#ifndef DEF_GLOBAL
    extern void logMessage(LogType type, const std::string& tag, const std::string& message,
        LogOutput output = LogOutput::Console, const std::string& fileName = "");
#else
    void logMessage(LogType type, const std::string& tag, const std::string& message,
        LogOutput output = LogOutput::Console, const std::string& fileName = "") {
        std::string timestamp = getCurrentTime();
        std::string logEntry = "[" + timestamp + "] [" + logTypeToString(type) + "] [" + tag + "] " + message;

        if (output == LogOutput::Console) {
            std::cout << logEntry << std::endl;
        }
        else if (output == LogOutput::File && !fileName.empty()) {
            std::ofstream logFile;
            logFile.open(fileName, std::ios_base::app); // Append mode
            if (logFile.is_open()) {
                logFile << logEntry << std::endl;
                logFile.close();
            }
            else {
                std::cerr << "Error: Could not open file " << fileName << " for logging." << std::endl;
            }
        }
        else {
            std::cerr << "Error: Invalid log output or missing file name." << std::endl;
        }
    }
#endif

#ifndef DEF_GLOBAL
    extern void logMessage(const std::string& message,
        bool autoLineWrap = false,
        LogOutput output = LogOutput::Console,
        const std::string& fileName = "");
#else
    void logMessage(const std::string& message,
        bool autoLineWrap = false,
        LogOutput output = LogOutput::Console,
        const std::string& fileName = "") {
        std::string timestamp = getCurrentTime();

        if (output == LogOutput::Console) {
            std::cout << message;
            if (autoLineWrap) {
                std::cout << std::endl;
            }
        }
        else if (output == LogOutput::File && !fileName.empty()) {
            std::ofstream logFile;
            logFile.open(fileName, std::ios_base::app); // Append mode
            if (logFile.is_open()) {
                logFile << message;
                if (autoLineWrap) {
                    logFile << std::endl;
                }
                logFile.close();
            }
            else {
                std::cerr << "Error: Could not open file " << fileName << " for logging." << std::endl;
            }
        }
        else {
            std::cerr << "Error: Invalid log output or missing file name." << std::endl;
        }
    }
#endif

    class RawLogger {
    public:
        RawLogger(LogOutput output = LogOutput::Console, const std::string& fileName = "")
            : output(output), fileName(fileName) {}

        void setOutput(LogOutput newOutput, const std::string& newFileName = "") {
            output = newOutput;
            if (newFileName != fileName) {
                fileName = newFileName;
            }
        }

        void setFileName(const std::string& newFileName) {
            fileName = newFileName;
        }
        virtual void log(const std::string& message) const {
            logMessage(message, false, output, fileName);
        }

    protected:
        LogOutput output;
        std::string fileName;
    };

    class RawStreamLogger : public RawLogger {
    public:
        RawStreamLogger(LogOutput output = LogOutput::Console, const std::string& fileName = "")
            : RawLogger(output, fileName) {}

        RawStreamLogger& operator<<(const char* message) {
            log(message);
            return *this;
        }

        RawStreamLogger& operator<<(const std::string& message) {
            log(message);
            return *this;
        }

        template<typename T>
        RawStreamLogger& operator<<(const T& message) {
            log(std::to_string(message));
            return *this;
        }
    };

    class Logger : public RawLogger {
    public:
        Logger(LogOutput output = LogOutput::Console, const std::string& fileName = "")
            : RawLogger(output, fileName) {}

        void log(LogType type, const std::string& tag, const std::string& message) const {
            logMessage(type, tag, message, output, fileName);
        }

        void e(const std::string& tag, const std::string& message) const {
            logMessage(LogType::Error, tag, message, output, fileName);
        }

        void w(const std::string& tag, const std::string& message) const {
            logMessage(LogType::Warning, tag, message, output, fileName);
        }

        void i(const std::string& tag, const std::string& message) const {
            logMessage(LogType::Info, tag, message, output, fileName);
        }
    };

    class StreamLogger : public Logger {
    public:
        StreamLogger(LogType type, const std::string& tag, LogOutput output = LogOutput::Console, const std::string& fileName = "")
            : Logger(output, fileName), type(type), tag(tag) {}

        StreamLogger(const std::string& tag, LogOutput output = LogOutput::Console, const std::string& fileName = "")
            : Logger(output, fileName), type(LogType::Info), tag(tag) {}

        void setTag(const std::string& newTag) {
            tag = newTag;
        }

        StreamLogger& operator<<(LogType newType) {
            type = newType;
            return *this;
        }

        StreamLogger& operator<<(const char* message) {
            log(type, tag, message);
            return *this;
        }

        StreamLogger& operator<<(const std::string& message) {
            log(type, tag, message);
            return *this;
        }

        template<typename T>
        StreamLogger& operator<<(const T& message) {
            log(type, tag, std::to_string(message));
            return *this;
        }

    protected:
        LogType type;
        std::string tag;
    };
};

#endif
