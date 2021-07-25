#ifndef _THREAD_POOL_H
#define _THREAD_POOL_H

#include <iostream>
#include "../locker/locker.h"
#include "../log/block_queue.h"
#include "../mysql/sql_connection_pool.h"
#include "../basedefine/base_define.h"
#include <vector>
#include <string>
#include <thread>
#include <memory>

template <typename T>
class thread_pool {
public:
    //默认线程池数量为8
    thread_pool(int actor_model, connection_pool* conn_pool, int thread_num = 8, int max_request = 10000);
    ~thread_pool();
    bool append(T* req, int state);
    bool append_p(T* req);
    

private:
    static void * Worker(void* arg); //工作线程，让线程池里面的线程都注册为这个
    void Run();

private:
    size_t m_thread_num;
    size_t m_max_requests;
    vector<thread> m_threads;
    block_queue<T*> m_request_queue; //请求队列
    sem m_queuestat;
    connection_pool* m_conn_pool;
    int m_actor_model;

};

template <class T>
thread_pool<T>::thread_pool(int actor_model, connection_pool* conn_pool,
                            int thread_num, int max_request):m_thread_num(thread_num), m_max_requests(max_request),
                            m_conn_pool(conn_pool), m_actor_model(actor_model) {
    if (thread_num <= 0 || max_request <= 0) {
        throw std::exception();
    }
    for (int i = 0; i < thread_num; ++i) {
        thread t(Worker, this);
        t.detach();
        m_threads.push_back(std::move(t));
    }
    m_request_queue.resize(m_max_requests);
}


template <class T>
thread_pool<T>::~thread_pool(){
    m_threads.clear();
}

template <class T>
bool thread_pool<T>::append(T* req, int state) {
    if (m_request_queue.size() >= m_max_requests) {
        return false;
    }
    req->m_state = state; //一个http请求
    m_request_queue.push(req); //已经加锁了
    m_queuestat.post(); //信号量post
    return true;
}

template <class T>
bool thread_pool<T>::append_p(T* req) {
    if (m_request_queue.size() >= m_max_requests) {
        return false;
    }
    m_request_queue.push(req); //已经加锁了
    m_queuestat.post(); //信号量post
    return true;
}

template <class T>
void* thread_pool<T>::Worker(void* arg){
    thread_pool* tp = static_cast<thread_pool*>(arg);
    tp->Run();
    return tp;
}

template <class T>
void thread_pool<T>::Run() {

    while(true) {
        m_queuestat.wait(); //all block here
        if (m_request_queue.empty()) {
            continue;
        }
        T* req = nullptr;
        if (m_request_queue.pop(req) == false || req == nullptr) {
            continue;
        }
        //1 = reactor, 0 = proactor
        if (ReactorMode == m_actor_model) {
            if (ReadState == req->m_state) {
                req->m_isdeal = ConnIsDeal; // m_isdeal是线程处理了相应的sockfd后，将其置为ConnIsDeal
                if (req->ReadOnce() == true) { 
                    // std::cout << "read_once == true " << std::endl;
                    connectionRAII mysqlcon(&(req->m_mysql), m_conn_pool); //从sql连接池里面获取一个连接
                    req->process();
                } else {
                    req->m_timer_flag = TimerDisable; //means timer should be closed
                }
            } else {
                req->m_isdeal = ConnIsDeal;
                if (req->Write() == false) {
                    req->m_timer_flag = TimerDisable;
                }
            }
        } else {
            connectionRAII mysqlcon(&(req->m_mysql), m_conn_pool); //从sql连接池里面获取一个连接
            req->process();
        }
    }

}





#endif