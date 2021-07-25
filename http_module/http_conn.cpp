#include "http_conn.h"
#include <mysql/mysql.h>
#include <fstream>
#include <memory>

using namespace std;

namespace http {

//init static member;
int HttpConn::m_epollfd = -1;
uint64_t HttpConn::m_user_count = 0;


int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, bool one_shot, int trigmode){
    epoll_event event;
    event.data.fd = fd;

    if (1 == trigmode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;  //在对端（客户端）close时会触发EPOLLRDHUP事件，注册了就可以处理这个关闭
    else
        event.events = EPOLLIN | EPOLLRDHUP;
    if (one_shot)
        event.events |= EPOLLONESHOT;  //one_shot的具体功能，防止对同一事件的重复触发https://blog.csdn.net/liuhengxiao/article/details/46911129
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,nullptr);
    close(fd);
}

void modfd(int epollfd, int fd, int ev, int trigmode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == trigmode) {
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    } else {
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    }

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void HttpConn::InitSqlResult(connection_pool* connpool, string tablename) {
    m_mysql = nullptr;
    connectionRAII mysqlcon(&m_mysql, connpool);
    string select_sql("SELECT username,passwd FROM ");
    select_sql.append(tablename);

    if (mysql_query(m_mysql, select_sql.c_str()) != 0) {
        LOG_ERROR("SELECT error:%s", mysql_error(m_mysql));
    }

    //从表中检索完整的结果集
    MYSQL_RES* result = mysql_store_result(m_mysql);

    while (MYSQL_ROW row = mysql_fetch_row(result)) {
        string username(row[0]);
        string password(row[1]);
        sql_users[username] = password;
    }

    mysql_free_result(result);
}


void HttpConn::Init(int sockfd, const sockaddr_in &addr, const char* root_path, int trigmode, int log_status, string user, string passwd, string sqlname, string tablename){
    m_sockfd = sockfd;
    m_address = addr;
    addfd(m_epollfd, m_sockfd, true, trigmode);
    ++m_user_count;

    m_doc_root = root_path; //work path
    m_log_status = log_status;

    m_trig_mode = trigmode;

    m_sql_name = sqlname;
    m_sql_passwd = passwd;
    m_sql_user = user;
    m_sql_tablename = tablename;

    init();
}

void HttpConn::init() {
    m_mysql = nullptr;
    m_bytes_to_send = 0;
    m_bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_longconn = false;
    m_method = GET;
    m_url = "";
    m_host = "";
    m_version = "";
    m_content_length = 0;
    m_start_line = 0;
    m_check_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    m_post = 0;
    m_state = ReadState;
    m_timer_flag = TimerEnable; // timer refresh flag
    m_isdeal = ConnUnDeal; 

    bzero(m_read_buf, READ_BUFFER_SIZE);
    bzero(m_write_buf, WRITE_BUFFER_SIZE);
    m_real_file.clear();
}

LINE_STATUS HttpConn::parse_line(){
    char tmp;
    while(m_check_idx < m_read_idx) {
        tmp = m_read_buf[m_check_idx];
        if(tmp == '\r'){
            if(m_check_idx + 1 == m_read_idx){
                return LINE_OPEN;
            }else if(m_read_buf[m_check_idx+1] == '\n'){
                m_read_buf[m_check_idx++] = '\0';
                m_read_buf[m_check_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(tmp == '\n'){
            if(m_check_idx > 1 && m_read_buf[m_check_idx - 1] == '\r'){
                m_read_buf[m_check_idx-1] = '\0';
                m_read_buf[m_check_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        ++m_check_idx;
    }
    return LINE_OPEN; //line still unfinish
}


// make sure read buffer is bigger than content
bool HttpConn::ReadOnce(){
    if (m_read_idx >= READ_BUFFER_SIZE) {
        return false;
    }
    int bytes_read = 0;

    if (LevelTriggerMode == m_trig_mode) { // LT, LT mode only need read once and it can keep reading
        bytes_read = recv(m_sockfd, m_read_buf, READ_BUFFER_SIZE - m_read_idx, 0); //read once
        if(bytes_read <= 0) {
            LOG_ERROR("Read data error");
            return false;
        }
        m_read_idx += bytes_read;
        //std::cout << "m_read_buf = " << m_read_buf << std::endl;
        return true;
    } else { // ET 
        while(true) {
            bytes_read = recv(m_sockfd, m_read_buf, READ_BUFFER_SIZE - m_read_idx, 0);
            if (bytes_read < 0) {
                if(errno == EAGAIN || errno ==  EWOULDBLOCK) {
                    break;
                }
                return false;
            }else if (bytes_read == 0) {
                return false; //client close fd
            }
            m_read_idx += bytes_read;
        }
        std::cout << "m_read_buf = " << m_read_buf << std::endl;
        return true;
    }
}

HTTP_CODE HttpConn::parse_req_line(const char* line) {
    shared_ptr<char> method(new char[10],[](char* ptr){ delete[] ptr; });
    shared_ptr<char> url(new char[100],[](char* ptr){ delete[] ptr; });
    shared_ptr<char> version(new char[10],[](char* ptr){ delete[] ptr; });
    bzero(method.get(),10);
    bzero(url.get(),100);
    bzero(version.get(),10);
    sscanf(line, "%[^ ] %[^ ] %[^ ]", method.get(), url.get(), version.get());

    //cout << method.get() << " " << url.get() << " " << version.get() << endl;
    if (strcasecmp(method.get(), "GET") == 0) {
        m_method = GET;
    } else if (strcasecmp(method.get(), "POST") == 0) {
        m_method = POST; 
        m_post = 1;
    } else {
        cout << "method parse failure" << endl;
        return BAD_REQUEST; // only support get and post 
    }

    m_url = url.get();
    m_version = version.get();
    if (m_url.empty() || m_version.empty()) {
        cout << "url or version failure" << endl;
        return BAD_REQUEST;
    }
    if(m_url.size() == 1 && m_url == "/") {
        m_url.append("sign.html");
    } else if (m_url.size() >= 7 && m_url.substr(0,7) == "http://"){
        m_url = m_url.substr(7);
    } else if (m_url.size() >= 8 && m_url.substr(0,8) == "https://"){
        m_url = m_url.substr(8);
    }
    m_check_state = CHECK_STATE_HEADER;

    return NO_REQUEST;
}

HTTP_CODE HttpConn::parse_headers(const char* line) {
    
    if (line[0] == '\0') { //如果分析到空的一行 /r/n
        if(m_content_length != 0){
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST; //这个状态代表分析到请求头的最后一行了即 /r/n
    }
    if (strncasecmp(line, CONNECTION.c_str(), CONNECTION.size()) == 0 && strcasecmp(line + CONNECTION.size(), "keep-alive") == 0) {
        m_longconn = true;
    } else if (strncasecmp(line, CONTENTLEN.c_str(), CONTENTLEN.size()) == 0) {
        m_content_length = atoi(line + CONTENTLEN.size());
    } else if (strncasecmp(line, HOST.c_str(), HOST.size()) == 0) {
        m_host = line + HOST.size();
    } else {
        LOG_INFO("parse unknow header: %s", line);
    }
    return NO_REQUEST;
}

/*judge http req was parse compeletly*/
HTTP_CODE HttpConn::parse_content(char* content) {
    if (m_read_idx >= (m_content_length + m_check_idx)) {
        content[m_content_length] = '\0';
        m_string = content;
        return GET_REQUEST; //这个状态代表获得了post里面的内容, 对post的内容做进一步处理
    }
    return NO_REQUEST;
}


HTTP_CODE HttpConn::do_req() {
    m_real_file = m_doc_root;
    const char* p = strrchr(m_url.c_str(),'/');

    // cout << p << endl;
    // if(strcmp(p + 1, "www.baidu.com") == 0) { // means 302 
    //     m_real_file = p + 1;
    //     return NO_REQUEST;
    // }



    // p[1] = 2 : log in ; p[1] = 3 : register check , m_post == 1 means post
    if(m_post == 1 && ((p[1] == '2') || (p[1] == '3'))) {
        string name("");
        string passwd("");
        int cur;
        for (cur = 5; m_string[cur] != '&'; ++ cur) {
            name += m_string[cur];
        }
        cur += 10;
        passwd = m_string.substr(cur);
        //register check
        if (p[1] == '3') {
            string sql_insert("");
            sql_insert.append("INSERT INTO ");
            sql_insert.append(m_sql_tablename);
            sql_insert.append("(username, passwd) VALUES('");
            sql_insert.append(name);
            sql_insert.append("', '");
            sql_insert.append(passwd);
            sql_insert.append("')");
            
            if (sql_users.find(name) == sql_users.end()) {
                m_locker.lock();
                sql_users[name] = passwd;
                m_locker.unlock();
                int ret = mysql_query(m_mysql, sql_insert.c_str());
                if (ret == 0) {
                    m_url = "/log.html";
                }else {
                    m_url = "/registerError.html";
                }
            } else {
                m_url = "/registerError.html";
            }
        } else if (p[1] == '2') {
            if (sql_users.find(name) != sql_users.end() && sql_users[name] == passwd) {
                m_url = "/welcome.html";
            } else {
                m_url = "/logError.html";
            }
        }
    }
    if (p[1] == '0') {
        m_real_file.append("/register.html"); //register
    } else if (p[1] == '1') {
        m_real_file.append("/log.html"); //log
    } else if (p[1] == '5') {
        m_real_file.append("/picture.html"); //pic
    } else if (p[1] == '6') {
        m_real_file.append("/video.html"); // video
    } else if (p[1] == '7') {
        m_real_file.append("/otherOption.html");  // 路飞
    } else if (p[1] == '9') {
        return MOVE_REQUEST; // 302重定向
    } else {
        m_real_file.append(m_url);
    }

    
    if (stat(m_real_file.c_str(), &m_file_stat) < 0) {
        return NO_RESOURCE;// No resource
    } 
    if (!(m_file_stat.st_mode & S_IROTH)) {
        return FORBIDDEN_REQUEST;   //其他组读权限，如果该权限没有其他组读权限则是一个被禁止的请求
    }
    if (S_ISDIR(m_file_stat.st_mode)) {
        return BAD_REQUEST; //Dir file
    } 
    int fd = open(m_real_file.c_str(), O_RDONLY);

    //mmap
    m_file_address = static_cast<char*>(mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (m_file_address == nullptr) {
        LOG_ERROR("%s", "bad mmap");
        exit(1);
    }

    close(fd);

    return FILE_REQUEST;
}


void HttpConn::unmap() {
    if (m_file_address) {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = nullptr;
    }
}




HTTP_CODE HttpConn::process_read() {
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* text = nullptr;
    //一开始会因为 m_check_state != CHECK_STATE_CONTENT而一直执行
    //(line_status = parse_line())这个语句给read_buf短句以及分行，直到解析完头部信息
    // 前面的条件也就成立，继续执行
    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK)) {
        text = getline();
        m_start_line = m_check_idx;
        LOG_INFO("receive line: %s",text);
        switch(m_check_state) {
            case CHECK_STATE_REQUESTLINE: {
                ret = parse_req_line(text);
                if (ret == BAD_REQUEST) {
                    std::cout << "request parse failure" << std::endl;
                    return ret;
                }
                break;
            }
            case CHECK_STATE_HEADER: {
                ret = parse_headers(text);
                if (ret == BAD_REQUEST) {
                    std::cout << "headers parse failure" << std::endl;
                    return ret;
                } else if (ret == GET_REQUEST) {
                    return do_req(); //处理post请求
                } 
                break;
            }
            case CHECK_STATE_CONTENT: {
                ret = parse_content(text);
                if (ret == GET_REQUEST) {
                    return do_req(); 
                }
                line_status = LINE_OPEN;
                break;
            }
            default:
                return INTERNAL_ERROR;
        } //switch
    } //while

    return NO_REQUEST;
}


bool HttpConn::Write(){
    int tmp = 0;

    if (m_bytes_to_send == 0) {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_trig_mode);
        init(); //发送完毕重新初始化
        return true;
    }

    while (true) {
        tmp = writev(m_sockfd, m_iv, m_iv_count);
        if (tmp < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_trig_mode);
                return true;
            }
            unmap();
            return false;
        }
        m_bytes_to_send -= tmp;
        m_bytes_have_send += tmp;
        if (m_bytes_have_send >= m_iv[0].iov_len) {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (m_bytes_have_send - m_write_idx);
            m_iv[1].iov_len = m_bytes_to_send;
        } else {
            m_iv[0].iov_base = m_write_buf + m_bytes_have_send;
            m_iv[0].iov_len -= m_bytes_have_send;
        }

        if (m_bytes_to_send <= 0) {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_trig_mode);
            // if connection = keep-alive
            if (m_longconn) {
                init();
                return true;
            } else {
                return false;
            }
        }
    }
    
}

bool HttpConn::add_rsp(const char* format, ...) {
    if (m_write_idx >= WRITE_BUFFER_SIZE) {
        return false;
    }
    va_list arg_list;
    va_start(arg_list, format); // va_arg option
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list); // you can't over this buf size when you write once
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)) {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;

    va_end(arg_list);

    // LOG_DEBUG("write_buf:%s", m_write_buf);
    return true;
}

//rsp line
bool HttpConn::add_status_line(int status, const char* title) {
    return add_rsp("%s %d %s\r\n", "HTTP/1.1", status, title);
}
bool HttpConn::add_content_length(int content_len) {
    return add_rsp("Content-Length:%d\r\n", content_len);
}
bool HttpConn::add_content_type() {
    return add_rsp("Content-Type:%s\r\n", "text/html");
}
bool HttpConn::add_linger() {
    return add_rsp("Connection:%s\r\n", (m_longconn == true) ? "keep-alive" : "close");
}
bool HttpConn::add_blank_line() {
    return add_rsp("%s", "\r\n");
}
bool HttpConn::add_content(const char *content) {
    return add_rsp("%s", content);
}

bool HttpConn::add_location(const char* redirect_url) {
    return add_rsp("Location:%s\r\n", redirect_url);
}


bool HttpConn::add_headers(int content_len) {
    return add_content_length(content_len) && add_linger() && add_blank_line();
}
bool HttpConn::add_302headers() {
    return add_linger() && add_blank_line();
}

bool HttpConn::process_write(HTTP_CODE ret) {
    switch (ret) {
        case INTERNAL_ERROR: {
            add_status_line(500, error_500_title.c_str());
            add_headers(error_500_form.size());
            if (add_content(error_500_form.c_str()) == false) {
                return false;
            }
            break;
        } 
        case BAD_REQUEST: {
            add_status_line(404, error_404_title.c_str());
            add_headers(error_404_form.size());
            if(add_content(error_404_form.c_str()) == false) {
                return false;
            }
            break;
        }
        case NO_RESOURCE: {
            add_status_line(404, error_404_title.c_str());
            add_headers(error_404_form.size());
            if(add_content(error_404_form.c_str()) == false) {
                return false;
            }
            break;
        } case FORBIDDEN_REQUEST: {
            add_status_line(403, error_403_title.c_str());
            add_headers(error_403_form.size());
            if(add_content(error_403_form.c_str()) == false) {
                return false;
            }
            break;
        } case FILE_REQUEST: {
            add_status_line(200, ok_200_title.c_str());
            add_headers(m_file_stat.st_size);
            if (m_file_stat.st_size != 0) {
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;
                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                m_bytes_to_send = m_write_idx + m_file_stat.st_size;
                return true;
            }
        } case MOVE_REQUEST: {
            add_status_line(302, move_302_title.c_str());
            add_location("https://www.baidu.com");
            add_302headers();
            break;
        } 

        default:
            break;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    m_bytes_to_send = m_write_idx;
    return true;
}

void HttpConn::CloseConn(bool real_close) {
    if (real_close && (m_sockfd != -1)) {
        std::cout << "close " << m_sockfd << std::endl;
        --m_user_count;
        removefd(m_epollfd,m_sockfd);
        m_sockfd = -1;
    }

}




void HttpConn::process() {
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST) {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_trig_mode);
        return;
    } 
    bool write_ret = process_write(read_ret);
    if (write_ret == false) {
        CloseConn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_trig_mode); // epoll can write
}

}//namespace http
