#ifndef LOGGER_HPP
#define LOGGER_HPP
/**
 * 2025-4-22 moyoj
 * 异步输出日志函数
 */
#include <string>
#include <mutex>
#include "LockQueue.hpp"

#define DEBUG true

class Logger
{
public:
    enum LogLevel
    {
        INFO,  // 普通的日志信息
        ERROR, // 错误信息
    };
    // 获取日志的单例
    static Logger &GetInstance();
    // 设置日志级别
    void SetLogLevel(Logger::LogLevel level);
    // 设置写日志
    void Log(std::string msg);
    
private:
    Logger::LogLevel loglevel_myj;                // 日志级别
    LockQueue<std::string> lockqueue_myj; // 日志缓冲队列
    Logger();
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;
};

//定义打印日志的宏
#define LOG_INFO(format,...)\
    do{\
        char buf[1024]={0};\
        Logger &logger = Logger::GetInstance();\
        logger.SetLogLevel(Logger::LogLevel::INFO);\
        snprintf(buf,1024,format,##__VA_ARGS__);\
        logger.Log(buf);\
    }while(0);

#define LOG_ERROR(format,...) \
    do{\
        char buf[1024]={0};\
        Logger &logger = Logger::GetInstance();\
        logger.SetLogLevel(Logger::LogLevel::ERROR);\
        snprintf(buf,1024,format,##__VA_ARGS__);\
        logger.Log(buf);\
    }while(0);
    
#endif