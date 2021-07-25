#include "sql_connection_pool.h"
#include <pthread.h>


using namespace std;

//构造函数
connection_pool::connection_pool()
{
    m_cur_conn = 0;
    m_free_conn = 0;
}

//析购函数
connection_pool::~connection_pool()
{
    destoryPool();
}

void connection_pool::init(string host, string user, string password, string databasename, int port, int maxconn, int log_status)
{
    m_host = host;
    m_user = user;
    m_password = password;
    m_databasename = databasename;
    m_port = port;
    m_log_status = log_status;
    
    for(int i = 0; i < maxconn; ++i) //建立一个数据库连接池
    {
        MYSQL* conn = nullptr;
        conn = mysql_init(conn);
        if (nullptr == conn) { 
            LOG_ERROR("MySQL INIT ERROR!");
            exit(1);
        }
        conn = mysql_real_connect(conn, m_host.c_str(), m_user.c_str(), m_password.c_str(), m_databasename.c_str(), m_port, nullptr, 0);
        if(nullptr == conn) {
            LOG_ERROR("MySQL REAL CONNECT ERROR!");
            exit(1);
        }
        m_conn_list.push_back(conn);
        ++m_free_conn;
    }

    m_reserve = sem(m_free_conn); //带参构造
    m_max_conn = m_free_conn;
    LOG_INFO("%s", "sql connection pool init success!");
}


//从连接池里面获取一个连接
MYSQL* connection_pool::getConnection()
{
    MYSQL* conn = nullptr;
    if (0 == m_conn_list.size()) {
        return nullptr;
    }

    m_reserve.wait();
    m_locker.lock();

    conn = m_conn_list.front();
    m_conn_list.pop_front();

    --m_free_conn;
    ++m_cur_conn;


    m_locker.unlock();

    return conn;
}

//释放一个连接的操作
bool connection_pool::releaseConnection(MYSQL* conn)
{
    if (nullptr == conn) {
        return false;
    }

    m_locker.lock();
    m_conn_list.push_back(conn);
    --m_cur_conn;
    ++m_free_conn;
    m_locker.unlock();

    m_reserve.post();
    return true;

}

//销毁一个连接池
void connection_pool::destoryPool() {
    m_locker.lock();
    if(m_conn_list.size() > 0)
    {
        for(auto it = m_conn_list.begin(); it != m_conn_list.end(); ++it)
        {
            mysql_close(*it);
            *it = nullptr;
        }
        m_cur_conn = 0;
        m_free_conn = 0;
        m_conn_list.clear();
    }
    mysql_library_end();
    m_locker.unlock();
}


int connection_pool::getFreeConn()
{
    return this->m_free_conn;
}

//资源即时获取类
//获取一个连接
connectionRAII::connectionRAII(MYSQL** conn, connection_pool* connPool)
{
    *conn = connPool->getConnection();
    m_conn_raii = *conn;
    m_pool_raii = connPool;
}
//析购
connectionRAII::~connectionRAII()
{
    m_pool_raii->releaseConnection(m_conn_raii);
}


