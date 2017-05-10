// Creates a global logger
// metrics_queue is not public, so using it incurs more overhead


#ifndef LOGGER_HPP
#define LOGGER_HPP

#include "shared_queue.hpp"
#include <string>
#include <thread>
#include <iostream>
#include <iomanip>

using namespace DaaS;

class LightweightLogger {
private:
    class LogData {
    public:
        // std::string msg{};
        std::string msg{"Not initialized!!!"};
        template <typename ... Args>
        LogData(const char* debug_level, Args ... args) {
            std::ostringstream oss;
            oss << std::setw(8) << std::left << debug_level;
            auto dummy = (oss << ... << args) && 0;
            static_cast<void>(dummy);
            msg = oss.str();

        }
        LogData() = delete;
        // LogData(std::string msg) = delete;
        LogData(std::string msg): msg(msg) {}
    };

    std::atomic_bool _running;
#ifdef STATS
    unsigned int logs_processed {0};
#endif // STATS
    shared_queue<LogData, false> log_queue;
    std::thread logger_thread;
    void process_logs_thread() {
        debug("Starting process_logs_thread...");
        while(_running || log_queue.size()) {
            verbose("Log queue size ", log_queue.size());
            auto tmp_log = log_queue.get();
            if (tmp_log == nullptr) {
                debug("No logs to precess!!\n\tLeaving?");
                continue;
            }
            std::cout << tmp_log->msg << std::endl;
#ifdef STATS
            ++logs_processed;
            // If we want to use metrics_queue, we need to set it and check it to be not-nullptr
        }
        // We are out of the log handling loop
        // debug("Processed logs: ", logs_processed);
        std::cout << std::setw(8) << std::left << "DEBUG" << "Processed logs: " << logs_processed << std::endl;
#else
        }
#endif  // STATS
    }

public:
    LightweightLogger():
        _running(true),
        logger_thread(&LightweightLogger::process_logs_thread, this)
    {}
    ~LightweightLogger() {
        _running = false;
        log_queue.flush();
        logger_thread.join();
        if ( DEBUG_LEVEL > 1)
            std::cout << "============================================================" << std::endl
                 << "~LightweightLogger" << std::endl
                 << "============================================================" << std::endl ;
    }
    template <typename ... Args>
    void log(Args ... logs) {
        log_queue.put( std::unique_ptr<LogData>(new LogData("LOG", logs...)) );
    }
    template <typename ... Args>
    void debug(Args ... logs) {
        if ( DEBUG_LEVEL > 0)
            log_queue.put( std::unique_ptr<LogData>(new LogData("DEBUG", logs...)) );
    }
    template <typename ... Args>
    void trace(Args ... logs) {
        if ( DEBUG_LEVEL > 1)
            log_queue.put( std::unique_ptr<LogData>(new LogData("TRACE", logs...)) );
    }
    template <typename ... Args>
    void verbose(Args ... logs) {
        if ( DEBUG_LEVEL > 2)
            log_queue.put( std::unique_ptr<LogData>(new LogData("VERBOSE", logs...)) );
    }
};

//static LightweightLogger logger;
//typedef LightweightLogger log_handler;


#endif  // LOGGER_HPP
