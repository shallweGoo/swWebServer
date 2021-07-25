#ifndef _MY_HTTP_SERVER_HPP_
#define _MY_HTTP_SERVER_HPP_
//为my_http_server类，是epoll_reactor的继承

#include "my_epoll_reactor.hpp"
#include "my_events.hpp"
#include "my_http_events.hpp"



class my_http_events;

class my_http_server:public my_epoll_reactor
{
private:

    my_http_events* glb_http_events;

public:

    my_http_server();

    my_http_server(int port); //带参构造

    ~my_http_server();

    virtual void add_epoll(int fd,int event,void* arg); 

    virtual void remove_epoll(int fd,int event,void* arg);

    //virtual void start_epoll(bool); //开始

    //virtual void set_listen_socket(bool isblock);

    virtual void loop_epoll();

    virtual void accept_client(void* arg);

    virtual void set_event(void* ev,int& fd,void* arg); //初始化my_events


};

#endif
