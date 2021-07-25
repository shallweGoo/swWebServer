#pragma once

#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <error.h>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <vector>
#include "warp.hpp"


using namespace std;
const static int BUF_LEN = 1024;
const static int EP_LEN = 1024;
//epoll主要用于服务器端的配置

class my_epoll
{
protected:
    int e_fd;//用于创建红黑树返回的e_fd
    int listen_fd; //listen的socket
    int PORT;// 端口号
    char* buffer;//缓冲区
    epoll_event* events; //epoll结构数组
    struct sockaddr_in s_addr;
public:
    //全部默认使用IPV4
    my_epoll();
    my_epoll(int port); //指定端口号
    // my_epoll(int port,int buffer_size,int epoll_size);//指定端口号和缓冲区大小,监听事件
    virtual ~my_epoll();

    virtual void start_epoll(bool isblock); //开始进行epoll

    virtual void set_listen_socket(bool isblock);

    virtual void add_epoll(int fd,int event,void* arg); //将fd事件挂在epoll上

    virtual void remove_epoll(int fd,int event,void* arg); //将fd事件从epoll上摘下

    virtual void loop_epoll();

    void up(int fd,char* buf,int len);

    inline int get_listenFd(){return listen_fd;};

private:
    

};



