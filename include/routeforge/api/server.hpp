#pragma once

#include "routeforge/service/route_service.hpp"
#include <httplib.h>
#include <memory>

namespace routeforge {
namespace api {

class Server {
public:
    Server(service::RouteService& service);
    
    void start(int port);
    void stop();

private:
    void setup_routes();
    
    service::RouteService& service_;
    httplib::Server server_;
};

} // namespace api
} // namespace routeforge
