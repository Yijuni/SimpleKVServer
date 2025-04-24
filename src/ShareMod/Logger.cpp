#include "Logger.hpp"
#include <thread>
#include <iostream>
Logger &Logger::GetInstance()
{
    static Logger logger;
    return logger;
}

void Logger::SetLogLevel(LogLevel level)
{
    loglevel_myj = level;
}

void Logger::Log(std::string msg)
{
    if(!DEBUG) return;
    lockqueue_myj.push(msg);
}

Logger::Logger()
{
    lockqueue_myj.setCapacity(10000); // 设置缓冲大小
    // 启动专门的写日志线程
    std::thread writeLogTask([&]()
                             {
        for(;;){
            //获取当天的日期,取lockqueue的日志写入文件中
            time_t now = time(nullptr);
            tm *now_tm = localtime(&now);

            char fiel_name[128];
            sprintf(fiel_name,"%d-%d-%d-log.txt",now_tm->tm_year+1900,now_tm->tm_mon+1,now_tm->tm_mday);

            FILE* pf = fopen(fiel_name,"a+"); //a+就是没有这个文件
            if(pf==nullptr){
                std::cout<<"logger file :"<<fiel_name<<"open error"<<std::endl;
                exit(EXIT_FAILURE);
            }
            
            std::string msg = lockqueue_myj.pop();

            char time_buf[128];
            sprintf(time_buf,"%d-%d-%d => [%s]",now_tm->tm_hour,now_tm->tm_min,now_tm->tm_sec,(loglevel_myj==INFO?"info":"error"));
            msg.insert(0,time_buf);//插入具体的时分秒
            msg.append("\n");
            fputs(msg.c_str(),pf);
            fclose(pf);
        } });
    // 分离线程
    writeLogTask.detach();
}
