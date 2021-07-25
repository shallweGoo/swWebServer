#include "web_server_config.h"



WebServerConfig::WebServerConfig(){
    m_port = 9999; //default web server port

    m_trig_mode = List_Conn_LT_LT; // 4 options to chose trigmode -> listenfd: LT + connfd : LT 

    m_lisenfd_trig_mode = LevelTriggerMode; 

    m_conn_trig_mode = LevelTriggerMode; 

    m_actor_model = ProactorMode;

    m_linger = LingerDisable;// elegent close flag switch, default close this opt

    m_thread_num = 8;

    //sql
    m_sql_conn_num = 8;

    m_sql_port = 7777;

    m_sql_username = "root";

    m_sql_password = "960516"; //loggin passwd

    m_sql_databasename = "m_projection"; // database name

    m_sql_tablename = "sw_users";

    //log

    m_log_mode = LogAsyncMode; //sync mode

    m_log_status = LogEnable; // open it

    m_log_name = "./Log";

    //http
    m_hostname = "localhost";
};


void WebServerConfig::Parse(int argc,char* argv[]) {
    int opt;
    const char* str = "p:l:m:o:s:t:c:a";
    while ((opt = getopt(argc, argv, str)) != -1) {
        switch (opt)
        {
        case 'p':
        {
            m_port = atoi(optarg);
            break;
        }
        case 'l':
        {
            m_log_mode = atoi(optarg);
            break;
        }
        case 'm':
        {
            m_trig_mode = atoi(optarg);
            break;
        }
        case 'o':
        {
            m_lisenfd_trig_mode = atoi(optarg);
            break;
        }
        case 's':
        {
            m_sql_conn_num = atoi(optarg);
            break;
        }
        case 't':
        {
            m_thread_num = atoi(optarg);
            break;
        }
        case 'c':
        {
            m_log_status = atoi(optarg);
            break;
        }
        case 'a':
        {
            m_actor_model = atoi(optarg);
            break;
        }
        default:
            break;
        }
    }
}