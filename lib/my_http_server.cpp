#include "my_http_server.hpp" 
 



my_http_server::my_http_server()
{
    cout << " 我的http服务器默认构造 " << endl;
    glb_http_events = new my_http_events[EP_LEN];
}

my_http_server::my_http_server(int port):my_epoll_reactor(port)
{
    cout << " 我的http服务器带端口号构造 " << endl;
}


my_http_server::~my_http_server()
{
    cout << " 我的http服务器析购函数 " << endl;
    delete[] glb_http_events;
}



void my_http_server::loop_epoll()
{
    epoll_event tmp_event;
    bzero(&tmp_event,sizeof(tmp_event));

    //设置工作目录
    int ret = chdir("/home/shallwe/Linux_nw_learning/g_http/html/");
    if(ret == -1) Sys_err("改变工作目录失败！");

    int checkpos = 0;
    while(1)
    {
        //超时验证
        long now = time(NULL);
        for (int i = 0; i < 100; ++i, ++checkpos) 
        {         //一次循环检测100个。 使用checkpos控制检测对象
            if (checkpos == EP_LEN-1) checkpos = 0;           //轮询
            if (glb_http_events[checkpos].get_status() != 1)  continue;//不在红黑树 e_fd 上
                
            long duration = now - glb_http_events[checkpos].get_active();       //客户端不活跃时间

            if (duration >= 60)                                              //不活跃时长,10s
            {                                         
                cout << "第" << glb_http_events[checkpos].get_fd() << "个客户端超时已经关闭!" << endl;
                remove_epoll(e_fd,0,&glb_http_events[checkpos]);                   //将该客户端 从红黑树 g_efd移除
            }
        }

        /*监听红黑树e_fd, 将满足的事件的文件描述符加至events数组中, 1秒没有事件满足, 返回 0*/
        int ret_fd_num = epoll_wait(e_fd,events,EP_LEN,1000);

        if(ret_fd_num < 0) Sys_err("epoll_wait出错！");


        for(int i = 0;i<ret_fd_num;++i)
        {
            my_http_events* ev = static_cast<my_http_events*>(events[i].data.ptr);
            
            if(ev->get_fd() == listen_fd)
            {
                accept_client(ev);
                continue;
            }
            if((ev->get_events() & EPOLLIN) && (events[i].events & EPOLLIN))
            {
                ev->recv_info(this);//this是传入这个http服务器对象
            }
            if((ev->get_events() & EPOLLOUT) && (events[i].events & EPOLLOUT))
            {
                ev->send_info(this);
            }
        }

    }



}




void my_http_server::set_event(void* event_ptr,int& fd,void* arg)
{
    my_http_events* ev = static_cast<my_http_events*>(event_ptr);
    ev->set_protocalBuf(BUF_LEN); //多了设置协议号这一步
    ev->set_fd(fd);
    ev->set_status(0); //0为添加前的属性
    ev->set_bufSize(BUF_LEN);
    ev->set_active(time(NULL));
    ev->set_arg(ev);
    if( arg != NULL)
    {
        ev->set_clientInfo(*static_cast<sockaddr_in*>(arg));
    }
}



void  my_http_server::accept_client(void* arg)
{
    my_http_events* ev = static_cast<my_http_events*>(arg);

    //设置工作目录

    ev->set_rootDir("/home/shallwe/Linux_nw_learning/g_http/html/");
    //不封闭



    sockaddr_in c_addr;
    socklen_t c_len = sizeof(c_addr);
    memset(&c_addr,0,c_len);
    int i = 0;
    int client_fd = Accept(ev->get_fd(),(sockaddr*)&c_addr,&c_len);


    do
    {
        for(i = 0;i<EP_LEN;++i)
        {
            if(glb_http_events[i].get_status() == 0) break; //找到了那个空的数组位置 ,l_fd初始化status被设置为0
        }
        if(i == EP_LEN) 
        {
            cout << "可以连接的客户端已经到达上限！无法建立新的连接请求" << endl;
            return;
        }
        int flag = fcntl(client_fd,F_GETFL);
        fcntl(client_fd,F_SETFL,flag | O_NONBLOCK); //设置为非阻塞方式，和后面的ET触发对应

        set_event(&glb_http_events[i],client_fd,&c_addr); // 初始化

        add_epoll(client_fd,EPOLLIN|EPOLLET,&glb_http_events[i]);//将epoll_event.data.ptr设置为my_events，联系起来

    } while(0);

    time_t t = glb_http_events[i].get_active();
    tm* ptr = localtime(&t);

    cout << "文件描述符号为 "<<client_fd<<" 的客户端已经连接。"<<endl;
    cout << "客户端地址为: " << inet_ntoa(c_addr.sin_addr) << " ,客户端端口为: " << ntohs(c_addr.sin_port) << 
    " ,上次活跃的时间 = "<< asctime(ptr) <<endl;


}



void my_http_server::add_epoll(int fd,int event,void* arg)
{
    my_http_events* ev = static_cast<my_http_events*>(arg); //传进来的是地址
    epoll_event tmp_ev;
    int CTL_MODE;
    if(ev->get_status() == 1)
    {
        CTL_MODE = EPOLL_CTL_MOD; //如果该文件描述符已经存在，则属性设置为修改
    }
    else 
    {
        CTL_MODE = EPOLL_CTL_ADD; //如果该文件描述符不存在，则添加
        ev->set_status(1);
    }
    tmp_ev.data.ptr = ev; //其实就是传这个结构体本身,这一步将epoll_event和my_events联系在一起里
    tmp_ev.events = event;

    ev->set_events(event); //更新事件和本身
    ev->set_active(time(NULL));
    int ret = epoll_ctl(e_fd,CTL_MODE,ev->get_fd(),&tmp_ev);
    if(ret == -1)
    {
        cout << "add_epoll函数添加出错" << endl;
        return;
    }
    if(CTL_MODE == EPOLL_CTL_MOD)
    {
        cout << "修改epoll节点监听模式成功" << endl;
    }
    else if(CTL_MODE == EPOLL_CTL_ADD)
    {
        cout << "添加epoll节点成功" << endl;
    }
}


void my_http_server::remove_epoll(int fd,int event,void* arg)
{
    my_http_events* ev = static_cast<my_http_events*>(arg); //传进来的是地址
    epoll_event tmp_ev;
    tmp_ev.data.ptr = arg;
    tmp_ev.events = 0;
    if(ev->get_status() != 1)
    {
        return;
    }
    else
    {
        ev->set_status(0);
        ev->set_active(time(NULL)); //获取当前时间
        int ret = epoll_ctl(e_fd,EPOLL_CTL_DEL,ev->get_fd(),&tmp_ev);
        Close(ev->get_fd());
        if(ret == -1)
        {
            cout << "remove_epoll函数出错" << endl;
            return;
        }
        
    }

    cout << "从epoll上删除文件描述符为 = " << ev->get_fd() << " 的客户端，端口号为 = " << ntohs(ev->get_clientInfo().sin_port) << endl;

}