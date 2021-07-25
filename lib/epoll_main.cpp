#include <iostream>
#include <cstring>
#include <algorithm>
#include "my_epoll.hpp"
#include "my_epoll_reactor.hpp"
#include "my_http_server.hpp"

using namespace std;

int main(int argc,char* argv[])
{
    my_epoll_reactor* sever = new my_http_server();

    sever->start_epoll(false);
    
    delete sever;
    return 0;
}
