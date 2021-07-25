CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2
endif

server: main.cpp  ./timer/lst_timer.cpp ./http_module/http_conn.cpp ./log/log.cpp ./CGImysql/sql_connection_pool.cpp  web_server.cpp web_server_config.cpp
	$(CXX) -o server  $^ $(CXXFLAGS) -lpthread -lmysqlclient -std=c++11

clean:
	rm  -r server
