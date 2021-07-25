#ifndef _LOG_H
#define _LOG_H

#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <string>
#include <thread>
#include <string.h>
#include <stdarg.h>
#include <memory>
#include <mutex>
#include "block_queue.h"
#include "../basedefine/base_define.h"

//使用单例来构造log类

class Log 
{
public:
    static Log& getLogInstance()
    {
        static Log m_log; //初始化一次
        return m_log;
    }

    static void* flush_log_thread(void* args) //一个线程负责做取出日志并读到文件中的操作，必须设为静态函数
    {
        return Log::getLogInstance().async_write_log();
    }

    bool init(const char* file_name, int close_log, size_t log_buf_size, size_t split_lines, size_t max_queueSize = 1000);

    void write_log(int level, const char* format,...);

    void flush(void);//刷新函数

private:
    Log();
    ~Log();
    Log(Log&)=delete;
    Log& operator=(const Log&)=delete;

    void* async_write_log()
    {
        std::string single_log;
        while (m_log_queue->pop(single_log)) {
            std::lock_guard<std::mutex> lock(m_mutex);
            fputs(single_log.c_str(), m_fp);
        }
        return nullptr;
    }


private:
    std::shared_ptr<block_queue<std::string>> m_log_queue;
    std::mutex m_mutex;
    FILE* m_fp;            //用于打开文件的句柄
    int m_today;           //今日日期记录
    long long m_count;
    int m_log_status;
    bool m_is_async;       //是否异步的标志位
    size_t m_split_lines;//日志最大行数
    size_t m_line_count;   //行数记录
    char m_log_name[128];  //日志文件名
    char m_dir_name[128];  //目录名
    char* m_buf;
    size_t m_log_buf_size;
};

#define LOG_DEBUG(format, ...) if (LogEnable == m_log_status) { Log::getLogInstance().write_log(0, format, ##__VA_ARGS__); Log::getLogInstance().flush(); } //马上持久化到磁盘
#define LOG_INFO(format, ...) if (LogEnable == m_log_status) { Log::getLogInstance().write_log(1, format, ##__VA_ARGS__); Log::getLogInstance().flush(); }
#define LOG_WARN(format, ...) if (LogEnable == m_log_status) { Log::getLogInstance().write_log(2, format, ##__VA_ARGS__); Log::getLogInstance().flush(); }
#define LOG_ERROR(format, ...) if (LogEnable == m_log_status) { Log::getLogInstance().write_log(3, format, ##__VA_ARGS__); Log::getLogInstance().flush(); }


#endif  //_LOG_H
