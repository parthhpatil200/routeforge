#include "routeforge/service/route_service.hpp"
#include "routeforge/core/yen.hpp"
#include "routeforge/core/constraints.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <mutex>
#include <cstdlib>

#include "routeforge/storage/postgres_repository.hpp"

namespace {
std::string status_name(routeforge::core::PathStatus status) {
    return status == routeforge::core::PathStatus::ACTIVE ? "ACTIVE" :
           status == routeforge::core::PathStatus::REROUTED ? "REROUTED" : "UNROUTABLE";
}

nlohmann::json path_json(const routeforge::core::PathRecord& record) {
    return {{"id", record.id}, {"status", status_name(record.status)},
            {"total_weight", record.current_path.total_weight}, {"hops", record.current_path.hops},
            {"nodes", record.current_path.nodes}};
}
}

namespace routeforge {
namespace service {

RouteService::RouteService() : failure_manager_(graph_, registry_) {
#ifdef ROUTEFORGE_WITH_POSTGRES
    if (const char* database_url = std::getenv("DB_URL")) {
        repository_ = std::make_unique<storage::PostgresRepository>(database_url);
        if (auto topology = repository_->load_topology_json()) graph_.load_from_json(*topology);
    }
#endif
}

RouteService::~RouteService() = default;

void RouteService::load_topology(const std::string& json_str) {
    std::unique_lock lock(mutex_);
    graph_.load_from_json(json_str);
    registry_.clear();
#ifdef ROUTEFORGE_WITH_POSTGRES
    if (repository_) repository_->save_topology(graph_);
#endif
}

std::string RouteService::get_topology() const {
    std::shared_lock lock(mutex_);
    nlohmann::json j;
    j["nodes"] = nlohmann::json::array();
    for (const auto& [id, name] : graph_.nodes()) j["nodes"].push_back({{"id", id}, {"name", name}});
    j["links"] = nlohmann::json::array();
    for (const auto& [id, link] : graph_.links()) {
        j["links"].push_back({{"id", id}, {"src", graph_.get_node_name(link.src)}, {"dst", graph_.get_node_name(link.dst)},
            {"bandwidth_mbps", link.bandwidth_mbps}, {"latency_ms", link.latency_ms}, {"cost", link.cost},
            {"utilization", link.utilization}, {"up", link.up}});
    }
    return j.dump();
}

std::optional<core::PathId> RouteService::compute_path(const std::string& request_json) {
    auto req = nlohmann::json::parse(request_json);
    
    if (!req.contains("source") || !req.contains("target") || !req["source"].is_string() || !req["target"].is_string())
        throw std::invalid_argument("source and target must be strings");
    std::string src_name = req["source"];
    std::string dst_name = req["target"];
    std::string metric = req.value("metric", "cost");
    if (metric != "cost" && metric != "latency" && metric != "hop") throw std::invalid_argument("metric must be cost, latency, or hop");
    
    std::unique_lock lock(mutex_);
    auto src_opt = graph_.get_node_id(src_name);
    auto dst_opt = graph_.get_node_id(dst_name);
    
    if (!src_opt || !dst_opt) return std::nullopt;
    
    std::unique_ptr<core::WeightStrategy> strategy;
    if (metric == "latency") strategy = std::make_unique<core::LatencyWeight>();
    else if (metric == "hop") strategy = std::make_unique<core::HopWeight>();
    else strategy = std::make_unique<core::CostWeight>();
    
    core::Constraints constraints;
    double max_util = 1.0;
    double min_available = 0.0;
    std::vector<core::LinkId> excluded_links;
    std::vector<core::NodeId> excluded_nodes;
    if (req.contains("constraints")) {
        if (req["constraints"].contains("max_utilization")) {
            max_util = req["constraints"]["max_utilization"];
            if (max_util < 0 || max_util > 1) throw std::invalid_argument("max_utilization must be between 0 and 1");
            constraints.max_utilization(max_util);
        }
        if (req["constraints"].contains("min_available_mbps")) {
            min_available = req["constraints"]["min_available_mbps"];
            if (min_available < 0) throw std::invalid_argument("min_available_mbps must be non-negative");
            constraints.min_available_mbps(min_available);
        }
        if (req["constraints"].contains("exclude_links")) {
            for (const auto& value : req["constraints"]["exclude_links"]) {
                auto id = value.get<core::LinkId>(); excluded_links.push_back(id); constraints.exclude_link(id);
            }
        }
        if (req["constraints"].contains("exclude_nodes")) {
            for (const auto& value : req["constraints"]["exclude_nodes"]) {
                auto node = graph_.get_node_id(value.get<std::string>());
                if (!node) throw std::invalid_argument("excluded node does not exist");
                if (*node == *src_opt || *node == *dst_opt) throw std::invalid_argument("source or target cannot be excluded");
                excluded_nodes.push_back(*node);
            }
        }
    }
    
    core::Dijkstra dijkstra(graph_);
    auto base = constraints.build();
    auto predicate = [&](const core::Link& link) {
        return base(link) && std::find(excluded_nodes.begin(), excluded_nodes.end(), link.src) == excluded_nodes.end() &&
               std::find(excluded_nodes.begin(), excluded_nodes.end(), link.dst) == excluded_nodes.end();
    };
    auto path_opt = dijkstra.compute_path(*src_opt, *dst_opt, *strategy, predicate);
    
    if (path_opt) {
        core::PathRecord record;
        record.src = *src_opt;
        record.dst = *dst_opt;
        record.metric = metric;
        record.max_utilization = max_util;
        record.min_available_mbps = min_available;
        record.excluded_links = excluded_links;
        record.excluded_nodes = excluded_nodes;
        record.current_path = *path_opt;
        
        const auto id = registry_.add_path(record);
#ifdef ROUTEFORGE_WITH_POSTGRES
        if (repository_) repository_->save_path(*registry_.get_path(id), graph_);
#endif
        return id;
    }
    
    return std::nullopt;
}

std::string RouteService::get_path_json(core::PathId id) const {
    std::shared_lock lock(mutex_);
    const core::PathRecord* record = registry_.get_path(id);
    if (!record) return "{}";
    
    return path_json(*record).dump();
}

std::string RouteService::get_path_history_json(core::PathId id) const {
    std::shared_lock lock(mutex_);
    auto history = registry_.history(id);
    if (history.empty()) return "{}";
    nlohmann::json result = nlohmann::json::array();
    for (const auto& record : history) result.push_back(path_json(record));
    return result.dump();
}

bool RouteService::update_link(core::LinkId id, const std::string& patch_json) {
    auto patch = nlohmann::json::parse(patch_json);
    std::unique_lock lock(mutex_);
    if (!graph_.get_link(id)) return false;
    auto number = [&](const char* key) -> std::optional<double> { return patch.contains(key) ? std::optional<double>(patch.at(key).get<double>()) : std::nullopt; };
    graph_.update_link(id, number("bandwidth_mbps"), number("latency_ms"), number("cost"), number("utilization"));
#ifdef ROUTEFORGE_WITH_POSTGRES
    if (repository_) repository_->save_link(*graph_.get_link(id));
#endif
    return true;
}

bool RouteService::link_exists(core::LinkId id) const {
    std::shared_lock lock(mutex_);
    return graph_.get_link(id) != nullptr;
}

void RouteService::fail_link(core::LinkId id) {
    std::unique_lock lock(mutex_);
    failure_manager_.fail_link(id);
#ifdef ROUTEFORGE_WITH_POSTGRES
    if (repository_) {
        repository_->save_link(*graph_.get_link(id));
        repository_->save_link_event(id, "FAIL");
        for (const auto& record : registry_.records()) repository_->save_path(record, graph_);
    }
#endif
}

void RouteService::restore_link(core::LinkId id) {
    std::unique_lock lock(mutex_);
    failure_manager_.restore_link(id);
#ifdef ROUTEFORGE_WITH_POSTGRES
    if (repository_) {
        repository_->save_link(*graph_.get_link(id));
        repository_->save_link_event(id, "RESTORE");
    }
#endif
}

} // namespace service
} // namespace routeforge
