#include "web_server.h"


using namespace std;

namespace websv{

WebServer::WebServer() {
    char work_path[100] = {0};
    m_root = getcwd(work_path, sizeof(work_path));
    m_root.append(RESOURCE_FOLD_NAME);
    m_users = new http::HttpConn[MAX_FD];
    m_users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer() {
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[0]);
    close(m_pipefd[1]);
    delete[] m_users;
    delete[] m_users_timer;
    delete m_threadpool;
    LOG_INFO("%s","web server quit...");
}



bool WebServer::InitLog() {
    if(LogEnable == m_log_status) {
        if(LogAsyncMode == m_log_mode) {
            Log::getLogInstance().init(m_log_name.c_str(), m_log_status, LOG_BUF_SIZE, LOG_SPLITS_LINE, LOG_QUEUE_SIZE); //async
        } else {
            Log::getLogInstance().init(m_log_name.c_str(), m_log_status, LOG_BUF_SIZE, LOG_SPLITS_LINE, 0);
        }
    }
    return true;
}



bool WebServer::InitSql() {
    m_conn_pool = connection_pool::getInstance();
    m_conn_pool->init(m_hostname, m_sql_username, m_sql_password, m_sql_databasename, m_sql_port, m_sql_conn_num, m_log_status);
    m_users[0].InitSqlResult(m_conn_pool, m_sql_tablename);
    if (m_conn_pool == nullptr) {
        LOG_ERROR("%s\n","init sql connect pool failed");
        return false;
    }
    return true;
}

bool WebServer::InitThreadPool() {
    m_threadpool = new thread_pool<http::HttpConn>(m_actor_model, m_conn_pool, m_thread_num);
    if(m_threadpool == nullptr) {
        LOG_ERROR("%s","init thread pool failure!");
        return false;
    }
    LOG_INFO("%s","init thread pool success!")
    return true;
}

bool WebServer::InitTrigMode() {
    switch (m_trig_mode) {
        case List_Conn_LT_LT: {
            m_listenfd_trig_mode = LevelTriggerMode;
            m_conn_trig_mode = LevelTriggerMode;
            break;
        }
        case List_Conn_LT_ET: {
            m_listenfd_trig_mode = LevelTriggerMode;
            m_conn_trig_mode = EdgeTriggerMode;
            break;
        }
        case List_Conn_ET_LT: {
            m_listenfd_trig_mode = EdgeTriggerMode;
            m_conn_trig_mode = LevelTriggerMode;
            break;
        }
        case List_Conn_ET_ET: {
            m_listenfd_trig_mode = EdgeTriggerMode;
            m_conn_trig_mode = EdgeTriggerMode;
            break;
        }
        default:
            break;
    }

    return true;
}



bool WebServer::Init(WebServerConfig&& config) {
    //sql
    m_sql_username = config.m_sql_username;
    m_sql_password = config.m_sql_password;
    m_sql_databasename = config.m_sql_databasename;
    m_sql_tablename = config.m_sql_tablename;
    m_sql_conn_num = config.m_sql_conn_num;
    m_sql_port = config.m_sql_port;

    //log
    m_log_mode = config.m_log_mode;
    m_log_status = config.m_log_status;
    m_log_name = config.m_log_name;

    //web server
    m_port = config.m_port;
    m_actor_model = config.m_actor_model;
    m_linger = config.m_linger;
    m_hostname = config.m_hostname;


    //thread_pool
    m_thread_num = config.m_thread_num;


    assert( InitLog() && InitSql() && InitThreadPool() && InitTrigMode() );
    event_listen();
    LOG_INFO("%s","Initial web server success!");
    return true;
}


void WebServer::event_listen() {
    m_listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    if (m_linger == LingerDisable) {
        struct linger tmp; // after calling close() connection is shutdown direct;
        tmp.l_onoff = 0;
        tmp.l_linger = 1;
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    } else {
        struct linger tmp;
        tmp.l_onoff = 1;
        tmp.l_linger = 1;
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    int ret = 0;
    struct sockaddr_in listenfd_addr;
    bzero(&listenfd_addr, sizeof(listenfd_addr));
    listenfd_addr.sin_family = AF_INET;
    listenfd_addr.sin_port = htons(m_port);
    listenfd_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 网卡地址

    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(m_listenfd, (struct sockaddr*)&listenfd_addr, sizeof(listenfd_addr));
    assert(ret >= 0);
    ret = listen(m_listenfd, 8); //back_log = 8
    assert(ret >= 0);

    m_utils.init(TIMESLOT);


    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    m_utils.addfd(m_epollfd, m_listenfd, false, m_listenfd_trig_mode);

    http::HttpConn::m_epollfd = m_epollfd; // set connection epoll_fd

    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    m_utils.setnonblocking(m_pipefd[1]); //设置m_pipefd[1]为非阻塞写
    m_utils.addfd(m_epollfd, m_pipefd[0], false, 0);

    m_utils.addsig(SIGPIPE, SIG_IGN); //ignore sigpipe
    m_utils.addsig(SIGALRM, m_utils.sig_handler, false); // when sigalrm arrive trigger sig_handler
    m_utils.addsig(SIGTERM, m_utils.sig_handler, false);

    alarm(TIMESLOT); //起一个定时器

    //class tool
    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}

//
void WebServer::SetTimer(int connfd, struct sockaddr_in client_address) {
    m_users[connfd].Init(connfd, client_address, m_root.c_str(), m_conn_trig_mode, m_log_status,
                         m_sql_username, m_sql_password, m_sql_databasename, m_sql_tablename);

    // init client data 
    // create timer ,set cb function and time limit ;
    m_users_timer[connfd].address = client_address;
    m_users_timer[connfd].sockfd = connfd;

    util_timer* timer = new util_timer();
    timer->user_data = &m_users_timer[connfd];
    timer->cb_func = cb_func; // cb_func is using to disconnect http connection
    time_t cur = time(NULL); //system time
    timer->expire = cur + 3 * websv::TIMESLOT; //set new timeout

    m_users_timer[connfd].timer = timer;
    m_utils.m_timer_lst.add_timer(timer);

}


// refresh timer by adding 3 times timeslot then adjust it when the socket had actions

void WebServer::adjust_timer(util_timer* timer) {
    time_t cur = time(NULL);
    timer->expire = cur + 3 * websv::TIMESLOT;
    m_utils.m_timer_lst.adjust_timer(timer);

    LOG_INFO("%s", "adjust timer");
}


//delete timer action
void WebServer::delete_timer(util_timer* timer, int sockfd) {
    timer->cb_func(timer->user_data);
    if (timer) {
        m_utils.m_timer_lst.del_timer(timer);
    }
    LOG_INFO("close fd %d", m_users_timer[sockfd].sockfd);
}

// listenfd process
bool WebServer::deal_client_data() {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    // LT
    if (0 == m_listenfd_trig_mode) {
        int connfd = accept(m_listenfd, (struct sockaddr *)(&client_addr), &client_len);
        if (connfd < 0) {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        if (http::HttpConn::m_user_count >= MAX_FD) {
            m_utils.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        SetTimer(connfd, client_addr);
    } else { // ET , keep accepting
        while (true) {
            int connfd = accept(m_listenfd, (struct sockaddr*)(&client_addr), &client_len);
            if (connfd < 0) {
                LOG_ERROR("%s,errno is:%d", "accept error", errno);
                break;
            }
            if (http::HttpConn::m_user_count >= MAX_FD) {
                m_utils.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            SetTimer(connfd, client_addr);
        }
        return false;
    }
    return true;
}

bool WebServer::deal_with_signal(bool& timeout, bool& stop_server) {
    int ret = 0;
    char signals[100] = {0};
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0); //pipe is returning signals
    if (ret <= 0) {
        return false;
    } 
    for (int i = 0; i < ret; ++ i) {
        switch (signals[i]) {
            case SIGALRM : {
                timeout = true;
                break;
            } case SIGTERM: {
                stop_server = true;
                break;
            }
        }
    }
    return true;
}

void WebServer::deal_with_read(int sockfd) {
    util_timer* timer = m_users_timer[sockfd].timer;
    if (ReactorMode == m_actor_model) { //reactor
        if (timer) {
            adjust_timer(timer);
        } 
        m_threadpool->append(&(m_users[sockfd]), ReadState); // after append, thread race it
        //std::cout << "append success" << std::endl;
        while (true) {
            if (ConnIsDeal == m_users[sockfd].m_isdeal) {
                if (TimerDisable == m_users[sockfd].m_timer_flag) {
                    delete_timer(timer, sockfd);
                    m_users[sockfd].m_timer_flag = TimerEnable;
                }
                m_users[sockfd].m_isdeal = ConnUnDeal;
                break;
            }
        }
    } else { //proactor async 
        if (m_users[sockfd].ReadOnce()) {
            LOG_INFO("deal with the client(%s)", inet_ntoa(m_users[sockfd].GetAddress().sin_addr));
            m_threadpool->append_p(&(m_users[sockfd])); 
        }
    }
}

void WebServer::deal_with_write(int sockfd) {
    util_timer* timer = m_users_timer[sockfd].timer;
    if (ReactorMode == m_actor_model) { //reactor
        if (timer != nullptr) {
            adjust_timer(timer);
        }
        m_threadpool->append(&(m_users[sockfd]), WriteState);
        //std::cout << "append write state success " << std::endl;
        while (true) {
            if (ConnIsDeal == m_users[sockfd].m_isdeal) {
                if(TimerDisable == m_users[sockfd].m_timer_flag) {
                    delete_timer(timer, sockfd);
                    m_users[sockfd].m_timer_flag = TimerEnable;
                }
                m_users[sockfd].m_isdeal = ConnUnDeal;
                break;
            }
        }
    } else { // proactor
        if (m_users[sockfd].Write() == true) {
            std::cout << "write success" << std::endl;
            LOG_INFO("send data to the client(%s)", inet_ntoa(m_users[sockfd].GetAddress().sin_addr));
            if (timer) {
                adjust_timer(timer);
            }
        } else {
            delete_timer(timer, sockfd);
        }
    }
}

//server main
//base timer to run 
void WebServer::Run() {
    bool timeout = false;
    bool stop_server = false;

    std::cout << "**************************************" << std::endl;
    std::cout << "***           sw_server            ***" << std::endl;
    std::cout << "***           start(^_^)           ***" << std::endl;
    std::cout << "**************************************" << std::endl;
    std::cout << "Web Server Mode : " << (m_actor_model == ReactorMode ?"Reactor":"Proactor") << std::endl;
    std::cout << "Log Mode : " << (m_log_mode == LogAsyncMode ?"AsyncMode":"SyncMode") << std::endl;
    while (stop_server == false) {
        int num = epoll_wait(m_epollfd, m_events, MAX_EVENT_NUMBER, -1);
        // be interupted by signal when system call
        if (num < 0 && errno != EINTR) {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for (int i = 0; i < num; ++ i) {
            int sockfd = m_events[i].data.fd;
            if (sockfd == m_listenfd) {
                bool flag = deal_client_data();
                if (flag == false) {
                    continue;
                }
            } else if (m_events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) { // server close connection;
                util_timer* timer = m_users_timer[sockfd].timer;
                delete_timer(timer, sockfd);
            } else if ((sockfd == m_pipefd[0]) && (m_events[i].events & EPOLLIN)) { // signal read
                bool flag = deal_with_signal(timeout, stop_server);
                if (flag == false) {
                    LOG_ERROR("%s","deal with signal failure");
                }
            } else if (m_events[i].events & EPOLLIN) {
                deal_with_read(sockfd);
            } else if (m_events[i].events & EPOLLOUT) {
                deal_with_write(sockfd);
            }
        }
        if (timeout) {
            m_utils.timer_handler(); // restart timer
            LOG_INFO("%s","timer tick");
            timeout = false;
        }
    }
}


}// namespace websv
