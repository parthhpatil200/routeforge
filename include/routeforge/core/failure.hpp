#pragma once

#include "routeforge/core/graph.hpp"
#include "routeforge/core/dijkstra.hpp"
#include <unordered_map>
#include <set>
#include <memory>
#include <string>
#include <vector>

namespace routeforge {
namespace core {

using PathId = uint32_t;

enum class PathStatus {
    ACTIVE,
    REROUTED,
    UNROUTABLE
};

struct PathRecord {
    PathId id;
    NodeId src;
    NodeId dst;
    std::string metric; // "cost", "latency", "hop"
    double max_utilization = 1.0;
    double min_available_mbps = 0.0;
    std::vector<LinkId> excluded_links;
    std::vector<NodeId> excluded_nodes;
    
    Path current_path;
    PathStatus status = PathStatus::ACTIVE;
};

class PathRegistry {
public:
    PathId add_path(const PathRecord& record);
    PathRecord* get_path(PathId id);
    const PathRecord* get_path(PathId id) const;
    std::vector<PathRecord> history(PathId id) const;
    std::vector<PathRecord> records() const;
    void record_history(PathId id);
    void clear();
    
    // Updates link index when a path is modified
    void update_path_links(PathId id, const std::vector<LinkId>& old_links, const std::vector<LinkId>& new_links);
    
    std::set<PathId> get_paths_using_link(LinkId link_id) const;

private:
    std::unordered_map<PathId, PathRecord> paths_;
    std::unordered_map<PathId, std::vector<PathRecord>> history_;
    std::unordered_map<LinkId, std::set<PathId>> link_to_paths_;
    PathId next_path_id_ = 1;
};

class FailureManager {
public:
    FailureManager(Graph& graph, PathRegistry& registry) 
        : graph_(graph), registry_(registry), dijkstra_(graph) {}

    void fail_link(LinkId link_id);
    void restore_link(LinkId link_id);

private:
    void reroute_affected_paths(LinkId link_id);
    
    Graph& graph_;
    PathRegistry& registry_;
    Dijkstra dijkstra_;
};

} // namespace core
} // namespace routeforge
