#include "my_epoll.hpp"


my_epoll::my_epoll():PORT(8888)
{
    cout << "默认构造:IP地址为本机IP,端口号为8888,缓冲区大小为1024,最大监听数1024" << endl;

    bzero(&s_addr,sizeof(s_addr));
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(PORT);

    listen_fd = -1; //初始化为-1,说明此时没有fd;
    e_fd = epoll_create(EP_LEN);
    if(e_fd == -1) Sys_err("ep create failed!");


    buffer = new char[BUF_LEN];
    bzero(buffer,sizeof(char)*BUF_LEN);
    events = new epoll_event[EP_LEN];
    bzero(buffer,sizeof(epoll_event)*EP_LEN);
}

my_epoll::my_epoll(int port):PORT(port)
{
    cout << "指定端口构造:IP地址为本机IP,缓冲区大小为1024,最大监听数1024" << endl;

    bzero(&s_addr,sizeof(s_addr));
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(PORT);

    e_fd = epoll_create(EP_LEN);
    if(e_fd == -1) Sys_err("ep create failed!");
    listen_fd = -1; //初始化为-1,说明此时没有fd;

    // buffer.resize(BUF_LEN);
    // epoll_buffer.resize(EP_LEN);
}


// my_epoll::my_epoll(int port,int buffer_size,int epoll_size):PORT(port),BUF_LEN(buffer_size),EP_LEN(epoll_size)
// {
//     bzero(&s_addr,sizeof(s_addr));
//     s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
//     s_addr.sin_family = AF_INET;
//     s_addr.sin_port = htons(PORT);

//     e_fd = epoll_create(EP_LEN);
//     if(e_fd == -1) Sys_err("ep create failed!");
//     listen_fd = -1; //初始化为-1,说明此时没有fd;
    
//     buffer.resize(BUF_LEN);
//     epoll_buffer.resize(EP_LEN);

// }



void my_epoll::start_epoll(bool isblock) //目的是为了开始epoll流程,将l_fd添加到epoll_tree上
{
    set_listen_socket(isblock);
    loop_epoll();
}



void my_epoll::add_epoll(int fd,int event,void* arg)
{
    epoll_event ev;
    ev.data.fd = fd;
    ev.events = event;
    
    epoll_ctl(e_fd,EPOLL_CTL_ADD,fd,&ev);

}


void my_epoll::remove_epoll(int fd,int event,void* arg)
{
    epoll_event ev;
    ev.data.fd = fd;
    ev.events = 0;  //remove操作不用事件
    epoll_ctl(e_fd,EPOLL_CTL_DEL,fd,&ev);

}


void my_epoll::loop_epoll()
{
    
    int ret = -1;
    
    sockaddr_in c_addr;
    socklen_t c_len = sizeof(c_addr);
    bzero(&c_addr,c_len);
    epoll_event ev; //这些用临时变量就可以了，不为epoll里面的成员


    while(1)  //开始循环监听
    {
        ret = epoll_wait(e_fd,events,EP_LEN,0); //设置非阻塞监听
        if(ret == -1)
        {
            perror("epoll_wait failed!\0");
            exit(0);
        }
        for(int i = 0;i<ret;++i)
        {
            int r_fd = events[i].data.fd;
            if(r_fd == listen_fd) //说明有新的socket想要建立连接
            {
                cout << "new collection coming!" << endl; 
                int client_fd = Accept(r_fd,(sockaddr*)&c_addr,&c_len);
                add_epoll(client_fd,EPOLLIN|EPOLLET,NULL);
                // add_epoll(client_fd,EPOLLIN);
            }
            else
            {
                cout << "client requires coming!"<< endl; 
                int r_ret = Read(r_fd,buffer,BUF_LEN);
                if(r_ret > 0)
                {
                    up(r_fd,buffer,r_ret);
                }
                else if(r_ret == 0) //边缘触发return 0，缓冲区可能还有数据
                {
                    cout << "移除" << r_fd <<"个socket" << endl;
                    remove_epoll(r_fd,0,NULL);
                    Close(r_fd);
                }
            }
        }
    }

}

void my_epoll::up(int fd,char* buf,int len)
{
    for(int i = 0;i<len;++i)
    {
        buf[i] = toupper(buf[i]); 
    }
    Write(fd,buf,len);
}

void my_epoll::set_listen_socket(bool isblock)
{

    if(listen_fd == -1) listen_fd = Socket(AF_INET,SOCK_STREAM,0);//默认IPV4
    else return;

    if(!isblock)
    {
        int flag = fcntl(listen_fd,F_GETFL);
        fcntl(listen_fd,F_SETFL,flag|O_NONBLOCK);
    }

    //设置端口复用
    int opt = 1;
    setsockopt(listen_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    Bind(listen_fd,(const sockaddr*)&s_addr,sizeof(s_addr));
    Listen(listen_fd,128);

    add_epoll(listen_fd,EPOLLIN|EPOLLET,NULL);//将l_fd添加epoll数组里面
}


my_epoll::~my_epoll()
{
    cout << "析购epoll" << endl;
    Close(listen_fd);
    delete[] buffer;
    delete[] events;
}
