#include "lst_timer.h"


sort_timer_lst::sort_timer_lst(){
    head = nullptr;
    tail = nullptr;
}

sort_timer_lst::~sort_timer_lst(){
    util_timer* tmp = head;
    while(tmp){
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

void sort_timer_lst::add_timer(util_timer* timer){
    if(timer == nullptr){
        return;
    }
    if(head == nullptr){
        head = tail = timer;
        return;
    }
    if(timer->expire < head->expire){
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }

    add_timer(timer,head);
}

void sort_timer_lst::add_timer(util_timer* timer,util_timer* lst_head){
    util_timer* pre = lst_head;
    util_timer* tmp = pre->next;
    while(tmp){
        if(timer->expire < tmp->expire){
            pre->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = pre;
            break; 
        }
        pre = tmp;
        tmp = tmp->next;
    }
    if(tmp == nullptr){
        pre->next = timer;
        timer->prev = pre;
        timer->next = nullptr;
        tail = timer;
    }

}


//变大了才要调整
void sort_timer_lst::adjust_timer(util_timer* timer){ 
    if (timer == nullptr) {
        return;
    }

    util_timer* tmp = timer->next;

    if (!tmp || (timer->expire < tmp->expire)) { //时间从小到大排，此时已经不需要调整
        return; 
    }

    if (timer == head) {
        head = head->next;
        head->prev = nullptr;
        timer->next = nullptr;
        add_timer(timer,head);
    } else {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

void sort_timer_lst::del_timer(util_timer* timer) {
    if (timer == nullptr) {
        return;
    }
    if((timer == head) && (timer == tail)){
        delete timer;
        head = nullptr;
        tail = nullptr;
        return;
    }
    if (timer == head) {
        head = head->next;
        head->prev = nullptr;
        delete timer;
        return;
    }
    if (timer == tail) {
        tail = tail->prev;
        tail->next = nullptr;
        delete timer;
        return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

void sort_timer_lst::tick() {
    if (head == nullptr) {
        return;
    }
    time_t cur = time(nullptr);
    util_timer* tmp = head;
    while(tmp) {
        if (cur < tmp->expire) {
            break;
        }
        tmp->cb_func(tmp->user_data);
        head = tmp->next;
        if (head) {
            head->prev = nullptr;
        }
        delete tmp;
        tmp = head;
    }
}

void Utils::init(int timeslot) {
    m_timeslot = timeslot;
}

int Utils::setnonblocking(int fd) {
    int old_option = fcntl(fd,F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

void Utils::addfd(int epollfd, int fd, bool one_shot, int trigmode) {
    epoll_event event;
    event.data.fd = fd;

    if (EdgeTriggerMode == trigmode) { // 1 = ET , 0 = LT
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    } else {
        event.events = EPOLLIN | EPOLLRDHUP;
    }

    if(one_shot) {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}


// for register a sig
void Utils::addsig(int sig, void (handler)(int), bool restart) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler; //注册信号捕捉函数
    if (restart) {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&(sa.sa_mask));
    assert(sigaction(sig,&sa,nullptr) != -1);
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler(){
    m_timer_lst.tick();
    alarm(m_timeslot);
}


//sig handler
void Utils::sig_handler(int sig) {
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}



void Utils::show_error(int connfd, const char* info){
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int* Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

void cb_func(client_data* user_data){
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, nullptr);
    assert(user_data);
    close(user_data->sockfd);
    --http::HttpConn::m_user_count;
}