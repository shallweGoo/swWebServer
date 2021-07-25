#ifndef _SQL_CONNECTION_POLL_H
#define _SQL_CONNECTION_POLL_H

#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <list>
#include <mysql/mysql.h>
#include "../locker/locker.h"
#include "../log/log.h"
#include "../basedefine/base_define.h"


//单例
class connection_pool
{
public:
    MYSQL* getConnection(); //获取数据库连接的句柄
    bool releaseConnection(MYSQL* conn);//释放连接
    int getFreeConn();//获取连接
    void destoryPool(); //销毁所有连接

    static connection_pool* getInstance() //返回引用
    {
        static connection_pool m_connection_pool;
        return &m_connection_pool;
    };


    //初始化连接池需要的参数
    //1、url：主机地址
    //2、User：sql用户 (进入mysql需要输入的user,默认为root)
    //3、PassWord：同上，进入时需要的密码
    //4、DataBaseName：需要使用的数据库的名字
    //5、Port：使用的端口
    //6、MaxConn：最大连接数
    //7、log_status:日志状态
    void init(string host, string user, string password, string databasename, int port, int maxconn, int log_status); 


private:
    connection_pool();
    ~connection_pool();

    int m_max_conn; //最大连接数
    int m_cur_conn; //当前连接数
    int m_free_conn; //空闲连接数
    locker m_locker;
    list<MYSQL*> m_conn_list;
    sem m_reserve;

public:
    int m_port;
    int m_log_status; 
    std::string m_host; //主机地址
    std::string m_user;
    std::string m_password;
    std::string m_databasename;
};



//资源获取即初始化类,用来初始化

class connectionRAII {
public:
    connectionRAII(MYSQL** conn, connection_pool* connPool);
    ~connectionRAII();

private:
    MYSQL* m_conn_raii;
    connection_pool* m_pool_raii;
};





#endif