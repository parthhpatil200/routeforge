#include "routeforge/service/route_service.hpp"
#include "routeforge/api/server.hpp"
#include <iostream>
#include <cstdlib>

using namespace routeforge;

int main() {
    service::RouteService route_service;
    api::Server server(route_service);

    const char* port_env = std::getenv("PORT");
    int port = 8080;
    if (port_env) {
        port = std::stoi(port_env);
    }

    server.start(port);

    return 0;
}
