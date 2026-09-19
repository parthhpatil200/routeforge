#pragma once

#include "routeforge/core/graph.hpp"
#include "routeforge/core/failure.hpp"
#include <shared_mutex>
#include <memory>
#include <string>

namespace routeforge::storage { class PostgresRepository; }

namespace routeforge {
namespace service {

class RouteService {
public:
    RouteService();
    ~RouteService();

    void load_topology(const std::string& json_str);
    std::string get_topology() const;
    
    std::optional<core::PathId> compute_path(const std::string& request_json);
    std::string get_path_json(core::PathId id) const;
    std::string get_path_history_json(core::PathId id) const;
    bool update_link(core::LinkId id, const std::string& patch_json);
    bool link_exists(core::LinkId id) const;

    void fail_link(core::LinkId id);
    void restore_link(core::LinkId id);

private:
    core::Graph graph_;
    core::PathRegistry registry_;
    core::FailureManager failure_manager_;

    std::unique_ptr<storage::PostgresRepository> repository_;
    
    mutable std::shared_mutex mutex_;
};

} // namespace service
} // namespace routeforge
