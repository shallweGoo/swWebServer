#ifndef _MY_EPOLL_REACTOR_HPP
#define _MY_EPOLL_REACTOR_HPP

#pragma once

#include "my_epoll.hpp"
#include "warp.hpp"
#include "my_events.hpp"




using namespace std;

class my_events;

//不考虑动态设置回调函数了，根据事件来判断使用哪个回调函数

class my_epoll_reactor : public my_epoll
{
    
public:
    my_epoll_reactor();

    my_epoll_reactor(int port);

    ~my_epoll_reactor();

    virtual void add_epoll(int fd,int event,void* arg); //如果不是为了实现多态其实没必要设置虚函数

    virtual void remove_epoll(int fd,int event,void* arg);//在派生类中如果要重写该函数，则设置为虚函数

    virtual void start_epoll(bool); //开始

    virtual void set_listen_socket(bool isblock);

    virtual void loop_epoll();

    virtual void accept_client(void* arg);

    virtual void set_event(void* ev,int& fd,void* arg); //初始化my_events
    
private:

    my_events* glb_events;

};







#endif