#include "routeforge/core/failure.hpp"
#include "routeforge/core/constraints.hpp"
#include <iostream>
#include <algorithm>

namespace routeforge {
namespace core {

PathId PathRegistry::add_path(const PathRecord& record) {
    PathId id = next_path_id_++;
    paths_[id] = record;
    paths_[id].id = id;
    history_[id].push_back(paths_[id]);
    
    update_path_links(id, {}, record.current_path.hops);
    return id;
}

const PathRecord* PathRegistry::get_path(PathId id) const {
    auto it = paths_.find(id);
    return it == paths_.end() ? nullptr : &it->second;
}

std::vector<PathRecord> PathRegistry::history(PathId id) const {
    auto it = history_.find(id);
    return it == history_.end() ? std::vector<PathRecord>{} : it->second;
}

std::vector<PathRecord> PathRegistry::records() const {
    std::vector<PathRecord> result;
    result.reserve(paths_.size());
    for (const auto& [id, record] : paths_) result.push_back(record);
    return result;
}

void PathRegistry::record_history(PathId id) {
    if (const PathRecord* record = get_path(id)) history_[id].push_back(*record);
}

void PathRegistry::clear() { paths_.clear(); link_to_paths_.clear(); history_.clear(); next_path_id_ = 1; }

PathRecord* PathRegistry::get_path(PathId id) {
    auto it = paths_.find(id);
    if (it != paths_.end()) return &it->second;
    return nullptr;
}

void PathRegistry::update_path_links(PathId id, const std::vector<LinkId>& old_links, const std::vector<LinkId>& new_links) {
    for (LinkId l : old_links) {
        link_to_paths_[l].erase(id);
    }
    for (LinkId l : new_links) {
        link_to_paths_[l].insert(id);
    }
}

std::set<PathId> PathRegistry::get_paths_using_link(LinkId link_id) const {
    auto it = link_to_paths_.find(link_id);
    if (it != link_to_paths_.end()) {
        return it->second;
    }
    return {};
}

void FailureManager::fail_link(LinkId link_id) {
    Link* link = graph_.get_link(link_id);
    if (link && link->up) {
        graph_.set_link_status(link_id, false);
        reroute_affected_paths(link_id);
    }
}

void FailureManager::restore_link(LinkId link_id) {
    Link* link = graph_.get_link(link_id);
    if (link && !link->up) {
        graph_.set_link_status(link_id, true);
        // Could potentially re-optimize paths that are unroutable or suboptimal
    }
}

void FailureManager::reroute_affected_paths(LinkId link_id) {
    std::set<PathId> affected = registry_.get_paths_using_link(link_id);
    
    for (PathId pid : affected) {
        PathRecord* record = registry_.get_path(pid);
        if (!record) continue;
        
        std::unique_ptr<WeightStrategy> strategy;
        if (record->metric == "latency") strategy = std::make_unique<LatencyWeight>();
        else if (record->metric == "hop") strategy = std::make_unique<HopWeight>();
        else strategy = std::make_unique<CostWeight>();
        
        Constraints constraints;
        if (record->max_utilization < 1.0) {
            constraints.max_utilization(record->max_utilization);
        }
        if (record->min_available_mbps > 0) constraints.min_available_mbps(record->min_available_mbps);
        for (LinkId id : record->excluded_links) constraints.exclude_link(id);
        auto base = constraints.build();
        auto predicate = [&](const Link& candidate) {
            if (!base(candidate)) return false;
            return std::find(record->excluded_nodes.begin(), record->excluded_nodes.end(), candidate.src) == record->excluded_nodes.end() &&
                   std::find(record->excluded_nodes.begin(), record->excluded_nodes.end(), candidate.dst) == record->excluded_nodes.end();
        };
        
        auto new_path_opt = dijkstra_.compute_path(record->src, record->dst, *strategy, predicate);
        
        std::vector<LinkId> old_hops = record->current_path.hops;
        
        if (new_path_opt) {
            record->current_path = *new_path_opt;
            record->status = PathStatus::REROUTED;
            registry_.update_path_links(pid, old_hops, record->current_path.hops);
        } else {
            record->current_path = Path{};
            record->status = PathStatus::UNROUTABLE;
            registry_.update_path_links(pid, old_hops, {});
        }
        registry_.record_history(pid);
    }
}

} // namespace core
} // namespace routeforge
