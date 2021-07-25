#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include "../mysql/sql_connection_pool.h"
#include "../log/log.h"
#include "../locker/locker.h"
#include "../timer/lst_timer.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <unordered_map>
#include <string>

namespace http {

/* rsp content */
static const std::string ok_200_title = "OK";
static const std::string move_302_title = "Moved Temporarily";
static const std::string error_400_title = "Bad Request";
static const std::string error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
static const std::string error_403_title = "Forbidden";
static const std::string error_403_form = "You do not have permission to get file form this server.\n";
static const std::string error_404_title = "Not Found";
static const std::string error_404_form = "The requested file was not found on this server.\n";
static const std::string error_500_title = "Internal Error";
static const std::string error_500_form = "There was an unusual problem serving the request file.\n";

/* header content */
static const std::string CONNECTION = "Connection:";
static const std::string CONTENTLEN = "Content-Length:";
static const std::string HOST = "Host:";
static const std::string LANGUAGE = "Language:";

static locker m_locker;
static std::unordered_map<std::string,std::string> sql_users;

int setnonblocking(int fd);
void addfd(int epollfd, int fd, bool one_shot, int trigmode);
void removefd(int epollfd, int fd);
void modfd(int epollfd, int fd, int ev, int trigmode);


enum METHOD{
    GET = 0,
    POST,
    HEAD,
    PUT,
    DELETE,
    TRACE,
    OPTIONS,
    CONNECT,
    PATH
};

enum CHECK_STATE{
    CHECK_STATE_REQUESTLINE = 0,
    CHECK_STATE_HEADER,
    CHECK_STATE_CONTENT
};

enum HTTP_CODE{
    NO_REQUEST,
    GET_REQUEST,
    MOVE_REQUEST, //302
    BAD_REQUEST, // 400 
    NO_RESOURCE, // 404
    FORBIDDEN_REQUEST, // 403
    FILE_REQUEST,
    // HOST_REQUEST, //302之后的网址请求
    INTERNAL_ERROR,
    CLOSED_CONNECTION
};

enum LINE_STATUS{
    LINE_OK = 0,
    LINE_BAD,
    LINE_OPEN
};

static const int FILENAME_LEN = 200;
static const int READ_BUFFER_SIZE = 2048;
static const int WRITE_BUFFER_SIZE = 2048;

class HttpConn {
public:
    HttpConn() = default;
    ~HttpConn() = default;

public:
    void Init(int sockfd, const sockaddr_in &addr, const char* root_path, int trigmode, int log_status, string user, string passwd, string sqlname, string tablename);
    void CloseConn(bool real_close = true);
    void Run();
    bool ReadOnce(); // 确保一次能把请求头读进来
    bool Write();
    void process();
    sockaddr_in& GetAddress() {
        return m_address;
    }
    void InitSqlResult(connection_pool* connpool, string tablename);
    int GetSockfd() {return this->m_sockfd;}
    
    int m_timer_flag;
    int m_isdeal;


private: 
    void init();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE);
    HTTP_CODE parse_req_line(const char* line);
    HTTP_CODE parse_rsp_line(const char* line);
    HTTP_CODE parse_headers(const char* line);
    HTTP_CODE parse_content(char* content);
    HTTP_CODE do_req();
    char* getline() { return m_read_buf + m_start_line;};
    LINE_STATUS parse_line();
    void unmap(); // memory map
    bool add_rsp(const char* format,...);
    bool add_content(const char* content);
    bool add_status_line(int status,const char * title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();
    bool add_location(const char* redirect_url);
    bool add_302headers();

    
public:
    static int m_epollfd;
    static uint64_t m_user_count;
    MYSQL* m_mysql;
    int m_state; // read = 0, write = 1;


private:
    int m_sockfd;
    sockaddr_in m_address;
    char m_read_buf[READ_BUFFER_SIZE];
    int m_read_idx;
    int m_check_idx;
    int m_start_line;
    
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;
    CHECK_STATE m_check_state;
    METHOD m_method;
    string m_real_file;
    std::string m_url;
    std::string m_version;
    std::string m_host;
    int m_content_length;
    bool m_longconn;
    char* m_file_address;
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count; 

    int m_post; // post
    std::string m_string; //save req header
    size_t m_bytes_to_send;
    size_t m_bytes_have_send;
    std::string m_doc_root;
    
    int m_trig_mode;
    int m_log_status;

    std::string m_sql_user;
    std::string m_sql_passwd;
    std::string m_sql_name;
    std::string m_sql_tablename;

};

} // namespace http

#endif