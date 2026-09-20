#include "routeforge/service/route_service.hpp"
#include "routeforge/api/server.hpp"
#include <iostream>
#include <cstdlib>

using namespace routeforge;

int main() {
    try {
        std::cerr << "main: start" << std::endl;
        service::RouteService route_service;
        std::cerr << "main: route_service ready" << std::endl;
        api::Server server(route_service);
        std::cerr << "main: server ready" << std::endl;

        const char* port_env = std::getenv("PORT");
        int port = 8080;
        if (port_env) {
            port = std::stoi(port_env);
        }

        std::cerr << "main: about to listen on port " << port << std::endl;

        server.start(port);
        std::cerr << "main: server.start returned" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "RouteForge failed to start: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
