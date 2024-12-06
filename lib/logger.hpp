#ifndef SM_LOGGER_2024
#define SM_LOGGER_2024

#include <chrono>
#include <ostream>
#include <fstream>
#include <iostream>
#include <filesystem>

class Logger {
private:
    const std::string filename_;
    std::string fullpath;
    std::ofstream log_stream_;

    void get_time() {

        auto now = std::chrono::system_clock::now();
        std::time_t end_time = std::chrono::system_clock::to_time_t(now);

        // Create a tm structure for formatting
        std::tm* tm_ptr = std::localtime(&end_time);

        char buffer[100]; // Buffer for formatted time
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_ptr); // Format without newline

        log_stream_ << buffer;
    }

public:

    std::string create_file(const std::string& filename) {
        auto date = []() {
            time_t now = time(0);
            tm *localTime = localtime(&now);
            char dateStr[11];
            strftime(dateStr, sizeof(dateStr), "%d_%m_%Y", localTime);
            return std::string(dateStr);
        };
        std::string datetm = date();
        std::filesystem::path logs = "log/" + datetm;
        if (std::filesystem::create_directories(logs) || std::filesystem::exists(logs)) {
            fullpath = "log/" + datetm + "/" + filename;
        }
        else {
            fullpath = filename;
        }
        return fullpath;
    }

    Logger(const std::string& filename, const std::string& streamfile_filename) : filename_(filename), log_stream_(std::ofstream(create_file(streamfile_filename))) {}
    ~Logger() {
        if (log_stream_.is_open()) {
            log_stream_.close();
        }
    }

    void set_log_file(const std::string& streamfile_filename) {
        log_stream_.close();
        log_stream_ = std::ofstream(create_file(streamfile_filename));
    }

    template<typename T>
    void log_base(T value) {
        log_stream_ << value;
    }

    template<typename T, typename... Args>
    void log_base(T value, Args... args) {
        log_base(value);
        log_base(args...);
    }

    template<typename... Args>
    void log(Args... args) {
        log_stream_ << "[" << filename_ << "] ";
        log_base(args...);
        log_stream_ << " [";
        get_time();
        log_stream_ << "]" << std::endl;
    }
};


#endif
