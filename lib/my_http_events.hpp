#ifndef _MY_HTTP_EVENTS_HPP_
#define _MY_HTTP_EVENTS_HPP_

#pragma once
#include "my_events.hpp"
#include "warp.hpp"
#include "my_http_server.hpp"
#include <sys/stat.h>
#include <dirent.h>

using namespace std;

class my_http_server;

class my_http_events : public my_events
{
    
private:
    //int line_len;

    int file_status;

    char* file_type;

    char* protocal_buf; //用于存放协议部分

    char* root_dir;

    char method[16];

    char protocal[16];
        
    char path[256];

public:
    my_http_events();

    ~my_http_events();

 
    const char* get_fileType(){ return file_type;};

    int get_fileStatus(){ return file_status;};
    
    void set_fileStatus(int file_status) { this->file_status = file_status; };

    void set_protocalBuf(int protocal_buf){ this->protocal_buf = new char[BUF_LEN]; };

    void set_rootDir(char* root_dir){ this->root_dir = root_dir;};

    char* get_rootDir(){return root_dir;};



    void recv_info(void* arg);

    void send_info(void* arg);


    void send_error();//发送错误信息

    void send_file(const char* file); //发送文件

    void send_dir(const char* dir); //发送目录

    void send_http_head(int );//发送http协议头

    void http_request(const char* file);//接收http请求,解析

    int hexit(char c); //16进制转10进制

    void encode_str(char* to, int tosize, const char* from); //编码

    void decode_str(char* to, char *from); //解码

    void set_fileType(const char *file_name);//获取文件类型





};



#endif



