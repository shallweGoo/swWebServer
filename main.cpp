#include <iostream>
#include <memory>
#include "web_server_config.h"
#include "web_server.h"

int main(int argc,char* argv[]) {
    //config
    WebServerConfig config;
    config.Parse(argc,argv);

    //server
    std::shared_ptr<websv::WebServer> server(new websv::WebServer());    
    server->Init(std::move(config));
    server->Run();

    return 0;
}
