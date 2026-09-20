#include "routeforge/api/server.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

namespace routeforge {
namespace api {

Server::Server(service::RouteService& service) : service_(service) {
    setup_routes();
}

void Server::setup_routes() {
    server_.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"status": "ok"})", "application/json");
    });

    server_.Get("/topology", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(service_.get_topology(), "application/json");
    });

    server_.Post("/topology", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            service_.load_topology(req.body);
            res.set_content(R"({"status": "loaded"})", "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(nlohmann::json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    server_.Post("/paths/compute", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto path_id_opt = service_.compute_path(req.body);
            if (path_id_opt) {
                res.set_content(nlohmann::json{{"path_id", *path_id_opt}}.dump(), "application/json");
            } else {
                res.status = 422;
                res.set_content(R"({"error": "No feasible path found"})", "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(nlohmann::json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    server_.Patch(R"(/links/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            const core::LinkId id = std::stoul(req.matches[1]);
            if (!service_.update_link(id, req.body)) {
                res.status = 404;
                res.set_content(R"({"error":"Link not found"})", "application/json");
                return;
            }
            res.set_content(R"({"status":"updated"})", "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(nlohmann::json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    server_.Get(R"(/paths/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        core::PathId id = std::stoul(req.matches[1]);
        std::string path_json = service_.get_path_json(id);
        if (path_json == "{}") {
            res.status = 404;
            res.set_content(R"({"error": "Path not found"})", "application/json");
        } else {
            res.set_content(path_json, "application/json");
        }
    });

    server_.Get(R"(/paths/(\d+)/history)", [this](const httplib::Request& req, httplib::Response& res) {
        const core::PathId id = std::stoul(req.matches[1]);
        const std::string history = service_.get_path_history_json(id);
        if (history == "{}") {
            res.status = 404;
            res.set_content(R"({"error":"Path not found"})", "application/json");
        } else {
            res.set_content(history, "application/json");
        }
    });

    server_.Post(R"(/links/(\d+)/fail)", [this](const httplib::Request& req, httplib::Response& res) {
        core::LinkId id = std::stoul(req.matches[1]);
        if (!service_.link_exists(id)) {
            res.status = 404; res.set_content(R"({"error":"Link not found"})", "application/json"); return;
        }
        service_.fail_link(id);
        res.set_content(R"({"status": "link failed and paths rerouted"})", "application/json");
    });
    
    server_.Post(R"(/links/(\d+)/restore)", [this](const httplib::Request& req, httplib::Response& res) {
        core::LinkId id = std::stoul(req.matches[1]);
        if (!service_.link_exists(id)) {
            res.status = 404; res.set_content(R"({"error":"Link not found"})", "application/json"); return;
        }
        service_.restore_link(id);
        res.set_content(R"({"status": "link restored"})", "application/json");
    });
}

void Server::start(int port) {
    std::cout << "Starting server on port " << port << std::endl;
    if (!server_.listen("0.0.0.0", port)) {
        std::cerr << "Failed to start server on port " << port << std::endl;
    }
}

void Server::stop() {
    server_.stop();
}

} // namespace api
} // namespace routeforge
