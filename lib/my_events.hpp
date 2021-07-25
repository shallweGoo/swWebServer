#ifndef _MY_EVENTS_HPP_
#define _MY_EVENTS_HPP_

#include <iostream>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <cstring>
#include "warp.hpp"
#include "my_epoll_reactor.hpp"

using namespace std;

class my_epoll_reactor;


class my_events
{
protected:
    int fd;                                                 //要监听的文件描述符,listen_fd和client_fd
    int events;                                             //对应的监听事件
    void *arg;                                              //泛型参数
    int status;                                             //是否在监听:1->在红黑树上(监听), 0->不在(不监听)
    char* buf;                                              // 事件的数组结构
    long last_active;                                       //记录每次加入红黑树 g_efd 的时间值
    int buf_size;                                           //记录buf大小字节
    int recv_len;                                           //记录接收的字长
    sockaddr_in client_addr;                              //记录客户端信息

public: 
    my_events();

    ~my_events();
    
    void set_status(const int& status){ this->status = status;};

    int get_status(){ return status; };

    void set_fd(const int& fd){this->fd = fd;};

    int get_fd(){ return fd; };

    void set_events(const int& events){ this->events = events; };

    int get_events(){ return events;};

    void set_active(const long& last_active){ this->last_active = last_active;};

    long get_active(){ return last_active; };

    void set_bufSize(const int & buf_size){ this->buf_size = buf_size; buf = new char [buf_size]; };

    int get_bufSize(){ return buf_size; }

    void set_recvLen(const long& recv_len){ this->recv_len = recv_len;};

    int get_recvLen(){ return recv_len; };

    void set_arg(void* arg){ this->arg = arg;};

    void* get_arg(){ return arg; };

    void set_clientInfo(sockaddr_in client_addr){ this->client_addr = client_addr; };

    sockaddr_in get_clientInfo(){ return client_addr; };

    
    void recv_info(void* arg); 
    
    void send_info(void* arg);

};



#endif