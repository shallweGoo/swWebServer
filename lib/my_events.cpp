#include "my_events.hpp"



my_events::my_events()
{
    fd = -1;                                                //要监听的文件描述符
    events = 0;                                             //对应的监听事件
    arg = NULL;                                             //泛型参数
    // call_back = NULL;                                       //回调函数
    status = 0;                                             //是否在监听:1->在红黑树上(监听), 0->不在(不监听)
    buf = NULL;
    recv_len = 0;                                                //记录buf的大小
    last_active = time(NULL);                               //记录每次加入红黑树 g_efd 的时间值 
    buf_size = 0;                 
    bzero(&client_addr,sizeof(client_addr));                
}





void my_events::recv_info(void *arg) //接收数据,为客户端传来的数据
{
    my_epoll_reactor* reactor = static_cast<my_epoll_reactor*>(arg);

    this->recv_len = recv(this->fd,this->buf,this->buf_size,0);

    if(this->recv_len == -1)
    {
        cout << "读取客户端输入失败!" << endl;
    }
    else if(this->recv_len == 0)
    {
        reactor->remove_epoll(this->fd,0,this);
        return;
    }
    else
    {
        reactor->add_epoll(this->fd,EPOLLOUT,this); 
    }

}

void my_events::send_info(void *arg) //发送数据
{
    my_epoll_reactor* reactor = static_cast<my_epoll_reactor*>(arg);

    int ret = send(this->fd,this->buf,this->recv_len,0);

    if(ret == -1)
    {
        reactor->remove_epoll(this->fd,0,this);
        cout << "写入客户端失败!" << endl;
    }
    else 
    reactor->add_epoll(this->fd,EPOLLIN|EPOLLET,this); 


}




my_events::~my_events()
{
    cout << "事件类的析购函数" << endl;
    delete[] buf;
    
}



