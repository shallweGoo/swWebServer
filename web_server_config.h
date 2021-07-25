#ifndef _WEB_SERVER_CONFIG_H
#define _WEB_SERVER_CONFIG_H

#include <stdlib.h>
#include <unistd.h>
#include <string>
#include "basedefine/base_define.h"





class WebServerConfig {
public:
    WebServerConfig();
    ~WebServerConfig() = default;

public:
    void Parse(int argc,char* argv[]);

public:
    int m_port; //web server port

    int m_trig_mode; // web server trigger mode
    int m_lisenfd_trig_mode;
    int m_conn_trig_mode;
    int m_linger; //close option
    
    int m_thread_num;
    int m_actor_model;

    // sql
    std::string m_sql_username; //database loggin name
    std::string m_sql_password; //loggin passwd
    std::string m_sql_databasename; // database name
    std::string m_sql_tablename; //table name
    int m_sql_conn_num;
    int m_sql_port;
    std::string m_hostname;
    
    //log
    int m_log_mode;
    int m_log_status;
    std::string m_log_name;

    
    

};





#endif