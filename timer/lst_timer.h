#ifndef _LST_TIMER_H
#define _LST_TIMER_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string>
#include <pthread.h>
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <time.h>

#include "../log/log.h"
#include "../http_module/http_conn.h"



class util_timer;

//客户端数据
struct client_data{
    sockaddr_in address;
    int sockfd;
    util_timer* timer;
};



class util_timer{
public:
    util_timer():prev(nullptr),next(nullptr){};
    ~util_timer() = default;

public:
    time_t expire;
    void (* cb_func)(client_data*);
    client_data* user_data;
    util_timer* prev;
    util_timer* next;
};


class sort_timer_lst{
public:
    sort_timer_lst();
    ~sort_timer_lst();

    void add_timer(util_timer* timer);
    void adjust_timer(util_timer* timer);
    void del_timer(util_timer* timer);
    void tick();

private:
    void add_timer(util_timer* timer,util_timer* lst_head);
    util_timer* head;
    util_timer* tail;

};

//定时器对象的句柄
class Utils {
public:
    Utils() = default;
    ~Utils() = default;

    void init(int timeslot);

    int setnonblocking(int fd);

    void addfd(int epollfd,int fd,bool one_shot,int trigmode);

    static void sig_handler(int sig);

    void addsig(int sig,void (handler)(int),bool restart = true);

    void timer_handler();

    void show_error(int connfd,const char* info);

public:
    static int* u_pipefd;
    sort_timer_lst m_timer_lst;
    static int u_epollfd;
    int m_timeslot;
};

void cb_func(client_data* user_data);

#endif