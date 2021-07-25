#include "my_http_events.hpp"



my_http_events::my_http_events()
{ 
    //cout << " my_http_events类默认构造 " << endl;
    //line_len = 0;
    file_type = NULL;
    file_status = -1;
    protocal_buf = NULL;
    root_dir = NULL;
}

my_http_events::~my_http_events()
{
    cout << " my_http_events类析购函数 " << endl;
    delete[] protocal_buf;
}


void my_http_events::recv_info(void *arg) //接收数据,解析浏览器传来的http请求,将协议内容都放到buf中
{
    my_http_server* hs = static_cast<my_http_server*>(arg);

    int len = get_line(this->fd,this->buf,this->buf_size); //获取一行数据，此时buf存着数据

    if(len == -1)    
    {
        cout << "解析http命令失败，buf无数据 " << endl;
    }
    else if(len == 0) //浏览器发完一次命令默认关闭
    {
        hs->remove_epoll(fd,0,this);
        return;
    }
    else //解析数据
    {
        sscanf(buf,"%[^ ] %[^ ] %[^ ]",method,path,protocal);
        cout << "http的请求方法: " << method << " 路径: "<< path << " 协议: "<< protocal << endl; 
        while(1) //把整个http协议从缓冲区读出来，清理缓冲区
        {
            len = get_line(fd,buf,buf_size);
            if(len == -1 || buf[0] == '\n')  break;//读到最后一个字符
        }
        bzero(buf,buf_size); //清空数组
        hs->add_epoll(this->fd,EPOLLOUT,this);
    }

}

void my_http_events::send_info(void *arg) //发送数据
{
    my_http_server* hs = static_cast<my_http_server*>(arg);

    if(strncasecmp(method,"GET",3) == 0)
    {
        cout << "收到http GET命令！" << endl;
        char* file = path+1; // 取出要访问的文件名

        //判断是否没有请求数据，如果是，则显示主页面            
        if(strcmp(path,"/") == 0)
        {
            file =(char*)"./";
        }
        http_request(file);
        /***********************/
        //必须要断开连接，断开连接的方式就是从epoll上移除下来

        memset(method,0,sizeof(method));
        memset(path,0,sizeof(path));
        memset(protocal,0,sizeof(protocal));

        hs->remove_epoll(fd,0,this); //等待下一次连接请求
    }

}

void my_http_events::http_request(const char* file)    //接收http请求,解析
{
    struct stat sbuf;
    int ret = stat(file,&sbuf);
    if(ret != 0) 
    {
        //回发404错误页面
        cout << "http occured 404 error!" << endl;
        set_fileStatus(404);
        send_error();
        return;
    }

    set_fileStatus(200);
    if(S_ISREG(sbuf.st_mode)) //说明是一个普通文件
    {
        //回发应答协议
        set_fileType(file);
        cout<< "这是一个 " << get_fileType() << " 类型文件！" <<endl;
        send_http_head(sbuf.st_size);
        send_file(file);
    }
    else if(S_ISDIR(sbuf.st_mode))//说明是一个目录文件,继续显示html页面
    {
        set_fileType(".html");
        cout<< "这是一个 " << get_fileType() << " 类型文件！" <<endl;
        //回发 给客户端请求数据
        send_http_head(-1);
        send_dir(file);
    }

}

void my_http_events::send_error() //发送错误请求
{

    int ret;

    send_http_head(-1); // 协议部分,发送协议头

    send_file("404.html"); // 发送一个做好的404 not found网页部分

    return;
}

void my_http_events::send_file(const char* file_name)//发送文件
{
    int f_fd = open(file_name, O_RDONLY);
    int read_len = 0;
    int ret = 0;
    while((read_len = Read(f_fd, buf, buf_size))> 0) //一直读
    {
        cout << "从文件中读到的字节数"<< read_len << endl;
        ret = send(fd,buf,read_len,0); //一直发送，发送出去了
        if(ret == -1)
        {
            if (errno == EAGAIN) 
            {
                perror("send error:");
                continue;
            } 
            else if (errno == EINTR) 
            {
                perror("send error:");
                continue;
            } 
            else 
            {
                perror("send error:");
                memset(buf,0,buf_size); //清空退出
                return;
            }
        }

    }
    cout << " 完整发送数据成功！" << endl;
    Close(f_fd); 

}

void my_http_events::send_dir(const char* dir_name) //发送目录
{
    
    // 拼一个html页面<table></table>
    memset(buf,0,buf_size);

    sprintf(buf, "<html><head><title>目录名: %s</title></head>", dir_name);
    sprintf(buf+strlen(buf), "<body><h1>当前目录: %s</h1><table>", dir_name);

    char enstr[1024] = {0};
    char full_path[1024] = {0};
    
    // 目录项二级指针
    struct dirent** ptr; 
    int num = scandir(dir_name, &ptr, NULL, alphasort); //ptr是malloc申请的，要手动释放，按照字典序排序
    

    // 遍历
    for(int i = 0; i < num; ++i) 
    {
        char* name = ptr[i]->d_name;

        // 拼接文件的完整路径
        sprintf(full_path, "%s/%s", dir_name, name);
        cout << " 目录中第 " << i << " 文件的路径为: " << full_path << endl;
        struct stat st;
        stat(full_path, &st);

		// 编码生成 %E5 %A7 之类的东西，用于汉字转化为unicode码，如果不是汉字的话也在函数中做了说明
        encode_str(enstr, sizeof(enstr), name);
        
        
        if(S_ISREG(st.st_mode)) // 如果是文件
        {       
            sprintf(buf+strlen(buf), 
                    "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",   
                    enstr, name, (long)st.st_size); //表格html生成语言
        } 
        else if(S_ISDIR(st.st_mode)) // 如果是目录
        {		       
            sprintf(buf+strlen(buf), 
                    "<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>",
                    enstr, name, (long)st.st_size);
        }
        int ret = send(fd, buf, strlen(buf), 0);
        if (ret == -1) 
        {
            if (errno == EAGAIN) 
            {
                perror("send error:");
                continue;
            } else if (errno == EINTR) 
            {
                perror("send error:");
                continue;
            } else {
                perror("send error:");
                bzero(buf,buf_size); //清空退出
                exit(1);
            }
        }

        memset(buf, 0, sizeof(buf));
        free(ptr[i]);

    }
    free(ptr);

    sprintf(buf+strlen(buf), "</table></body></html>");
    int ret = send(fd, buf, strlen(buf), 0);
    if(ret == -1)
    {
        cerr << "send error:";
        memset(buf, 0, sizeof(buf));
        return;
    }
    cout << "目录信息显示ok！" << endl; 


}

void my_http_events::send_http_head(int len) //发送协议头
{
    //清空一下协议缓冲区
    memset(protocal_buf,0,sizeof(protocal_buf));
    const char* title;
    if(file_status == 404)
    {
        title = "Not Found";
    }
    else if(file_status == 200)
    {
        title = "OK";
    }
    else
    {
        cout << "没有设其他的文件状态，send error 失败！ " << endl;
        return;
    }
    sprintf(protocal_buf,"HTTP/1.1 %d %s\r\n",file_status,title);
    sprintf(protocal_buf+strlen(protocal_buf),"Content-Type:%s \r\n",file_type);
    sprintf(protocal_buf+strlen(protocal_buf),"Content-Length:%d\r\n",len); //这个长度不确定可以写-1,没有关系
    sprintf(protocal_buf+strlen(protocal_buf),"Connection:close\r\n");
    sprintf(protocal_buf+strlen(protocal_buf),"\r\n");

    int ret = send(fd,protocal_buf,strlen(protocal_buf),0);

    if(ret == -1) Sys_err("send_error error");
    cout << "发送协议头成功！" << endl;

}

void my_http_events::set_fileType(const char *name)
{
    const char* dot;

    // 自右向左查找‘.’字符, 如不存在返回NULL
    dot = strrchr(name, '.');   
    if (dot == NULL)
        file_type = (char*)"text/plain; charset=utf-8";
    else if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        file_type = (char*)"text/html; charset=utf-8";
    else if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        file_type =  (char*)"image/jpeg";
    else if (strcmp(dot, ".gif") == 0)
        file_type = (char*)"image/gif";
    else if (strcmp(dot, ".png") == 0)
        file_type = (char*)"image/png";
    else if (strcmp(dot, ".css") == 0)
        file_type = (char*)"text/css";
    else if (strcmp(dot, ".au") == 0)
        file_type = (char*)"audio/basic";
    else if (strcmp( dot, ".wav" ) == 0)
        file_type = (char*)"audio/wav";
    else if (strcmp(dot, ".avi") == 0)
        file_type = (char*)"video/x-msvideo";
    else if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        file_type = (char*)"video/quicktime";
    else if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        file_type = (char*)"video/mpeg";
    else if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        file_type = (char*)"model/vrml";
    else if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        file_type = (char*)"audio/midi";
    else if (strcmp(dot, ".mp3") == 0)
        file_type = (char*)("audio/mpeg");
    else if (strcmp(dot, ".ogg") == 0)
        file_type = (char*)"application/ogg";
    else if (strcmp(dot, ".pac") == 0)
        file_type = (char*)"application/x-ns-proxy-autoconfig";
}


int my_http_events::hexit(char c) //16进制转10进制
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return 0;
}


/*
 *  这里的内容是处理%20之类的东西！是"解码"过程。
 *  %20 URL编码中的‘ ’(space)
 *  %21 '!' %22 '"' %23 '#' %24 '$'
 *  %25 '%' %26 '&' %27 ''' %28 '('......
 *  相关知识html中的‘ ’(space)是&nbsp
 */

//用于汉字的编码，和url配对，url需要把汉字转化为unicode码
void my_http_events::encode_str(char* to, int tosize, const char* from)
{
    for (int tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from) 
    {    
        //char *strchr(const char *str, int c)
        //在参数str所指向的字符串中搜索第一次出现字符c（一个无符号字符）的位置。
        if (isalnum(*from) || strchr("/_.-~", *from) != (char*)0)  //is_alnum判断一个字母是否为字母和数字
        {      
            *to = *from;
            ++to;
            ++tolen;
        }
        else
        {
            sprintf(to, "%%%02x", (int) *from & 0xff); //编码生成html中的字符
            to += 3;
            tolen += 3;
        }
    }
    *to = '\0';
}

void my_http_events::decode_str(char *to, char *from)
{
    for ( ; *from != '\0'; ++to, ++from  ) 
    {     
        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) 
        {       
            *to = hexit(from[1])*16 + hexit(from[2]);
            from += 2;                      
        } 
        else 
        {
            *to = *from;
        }
    }
    *to = '\0';
}


