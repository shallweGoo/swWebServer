#ifndef WEB_SERVER_H
#define WEB_SERVER_H


#include <iostream>
#include <string>
#include "./http_module/http_conn.h"
#include "./mysql/sql_connection_pool.h"
#include "./thread_pool/thread_pool.h"
#include "./basedefine/base_define.h"
#include "./timer/lst_timer.h"
#include "./log/log.h"
#include "./web_server_config.h"



namespace websv{


const size_t MAX_FD = 0xff;
const size_t MAX_EVENT_NUMBER = 1e4;
const std::string RESOURCE_FOLD_NAME = "/doc";
const int TIMESLOT = 5;


const size_t LOG_BUF_SIZE = 2048;
const size_t LOG_SPLITS_LINE = 8e4;
const size_t LOG_QUEUE_SIZE = 10;



class WebServer {
public:
    WebServer();
    ~WebServer();

    
public:
    //初始化类
    bool Init(WebServerConfig&& config);
    bool InitLog();
    bool InitSql();
    bool InitThreadPool();
    bool InitTrigMode();
    void SetTimer(int connfd, struct sockaddr_in client_address);

    //运行
    void Run();

private:
    void event_listen();
    
    //定时器类
    void adjust_timer(util_timer* timer);
    void delete_timer(util_timer* timer, int sockfd);

    //业务类
    bool deal_client_data(); //listen的业务
    bool deal_with_signal(bool& timeout, bool& stop_server); // 信号业务，超时删除功能
    void deal_with_read(int sockfd); // 读业务
    void deal_with_write(int sockfd); // 写业务


private:
    //webserver类
    std::string m_root; 
    int m_port;
    int m_actor_model;
    int m_trig_mode;

    epoll_event m_events[MAX_EVENT_NUMBER];

    //日志类
    int m_log_mode;   
    int m_log_status;
    std::string m_log_name;



    //数据库相关类
    std::string m_hostname;
    std::string m_sql_username;
    std::string m_sql_password;
    std::string m_sql_databasename;
    std::string m_sql_tablename;
    int m_sql_conn_num;
    int m_sql_port;
    connection_pool* m_conn_pool;
    


    //http连接类
    http::HttpConn* m_users;
    int m_epollfd;
    int m_listenfd;
    int m_pipefd[2];
    int m_listenfd_trig_mode;
    int m_conn_trig_mode;
    int m_linger;

    //定时器类
    client_data* m_users_timer;
    Utils m_utils;


    //线程池相关
    thread_pool<http::HttpConn>* m_threadpool;
    int m_thread_num;

};



}// namespace websv

#endif