#include "log.h"
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <sstream>
using namespace std;

Log::Log() {
    m_is_async = false; //代表同步触发模式
    m_line_count = 0;
    memset(m_log_name,0,sizeof(m_log_name));
    memset(m_dir_name,0,sizeof(m_dir_name));
}

Log::~Log() {
    if (m_fp != nullptr) {
        fclose(m_fp);
    }
    delete[] m_buf;
}

bool Log::init(const char* file_name, int log_status, size_t log_buf_size, size_t split_lines, size_t max_queueSize) {
    if (max_queueSize > 0) {
        m_is_async = true; //设置为异步日志模式，异步日志就是可以不用等待上一个写日志事件完成
        m_log_queue = make_shared<block_queue<string> >(max_queueSize);
        thread t(flush_log_thread, nullptr); //async thread
        t.detach();
    }

    m_log_status = log_status;
    m_split_lines = split_lines;
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];

    time_t t = time(NULL);
    tm* sys_tm = localtime(&t);
    tm my_tm = *sys_tm; //系统时间

    m_today = my_tm.tm_mday;//注入时间

    const char* real_file_name = strrchr(file_name, '/'); //从右向左匹配的第一个/符号

    ostringstream log_full_name;

    if (real_file_name == nullptr) {
        log_full_name << (my_tm.tm_year + 1900) << "_" << my_tm.tm_mon + 1 << "_" << my_tm.tm_mday << "_" << file_name;
    } else {
        strcpy(m_log_name, real_file_name+1); //日志文件名

        strncpy(m_dir_name, file_name, real_file_name - file_name + 1);//目录名

        log_full_name << m_dir_name << my_tm.tm_year + 1900 << "_"<< my_tm.tm_mon + 1 << "_" << my_tm.tm_mday << "_" << m_log_name;
    }

    m_fp = fopen(log_full_name.str().c_str(), "a");//以附加形式打开只写文件，不存在则建立，存在则附加

    if (m_fp == nullptr) {
        return false;
    }
    
    return true;
}

void Log::write_log(int level, const char* format, ...) {
    timeval now = {0,0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    tm *sys_tm = localtime(&t);
    tm my_tm = *sys_tm; //获取当前时间

    char s[16] = {0};

    switch(level) {
        case 0:
            strcpy(s, "[debug]:");
            break;
        case 1:
            strcpy(s, "[info]:");
            break;
        case 2:
            strcpy(s, "[warn]:");
            break;
        case 3:
            strcpy(s, "[erro]:");
            break;
        default:
            strcpy(s, "[info]:");
            break;
    }
    m_mutex.lock();
    ++m_count;

    //如果日志文件满了，要新建一个日志文件
    if(m_today != my_tm.tm_mday || m_count % m_split_lines == 0)
    {
        char new_log[256] = {0};
        fflush(m_fp); //持久化文件磁盘
        fclose(m_fp);
        char tail[16] = {0};

        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

        if(m_today != my_tm.tm_mday) {
            snprintf(new_log, 255, "%s%s%s", m_dir_name, tail, m_log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        } else {
            snprintf(new_log, 255, "%s%s%s.%lld", m_dir_name, tail, m_log_name, m_count / m_split_lines);
        }

        m_fp = fopen(new_log, "a");
    }

    m_mutex.unlock();

    va_list valst;
    va_start(valst, format); //获取format的地址，可变参数个数，初始化valst，想想函数栈帧就知道这个获取地址有什么用了
    
    string log_str;
    
    m_mutex.lock();
    
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
            my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
            my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

    int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, valst);


    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';

    log_str = m_buf;

    m_mutex.unlock();

    // 如果为异步，那就先放进队列里面,等到要被处理时才处理
    if (m_is_async && !m_log_queue->full()) {
        m_log_queue->push(log_str);
    } else {
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp); //如果为同步直接刷新到文件中，马上处理
        m_mutex.unlock();
    }

    va_end(valst);

}


void Log::flush(void)
{
    m_mutex.lock();
    //强制刷新写入流缓冲区
    fflush(m_fp);
    m_mutex.unlock();
}


