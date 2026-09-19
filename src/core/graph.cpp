#include "routeforge/core/graph.hpp"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace routeforge {
namespace core {

NodeId Graph::add_node(const std::string& name) {
    auto it = name_to_id_.find(name);
    if (it != name_to_id_.end()) {
        return it->second;
    }
    
    NodeId id = next_node_id_++;
    name_to_id_[name] = id;
    id_to_name_[id] = name;
    
    if (id >= adjacency_list_.size()) {
        adjacency_list_.resize(id + 1);
    }
    
    return id;
}

std::optional<NodeId> Graph::get_node_id(const std::string& name) const {
    auto it = name_to_id_.find(name);
    if (it != name_to_id_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::string Graph::get_node_name(NodeId id) const {
    auto it = id_to_name_.find(id);
    if (it != id_to_name_.end()) {
        return it->second;
    }
    return "";
}

void Graph::add_link(const Link& link) {
    if (link.src >= adjacency_list_.size() || link.dst >= adjacency_list_.size()) {
        throw std::invalid_argument("link references an unknown node");
    }
    if (link.bandwidth_mbps < 0 || link.latency_ms < 0 || link.cost < 0 ||
        link.utilization < 0 || link.utilization > 1) {
        throw std::invalid_argument("link metrics are out of range");
    }
    if (links_.count(link.id)) {
        throw std::invalid_argument("duplicate link id");
    }
    links_[link.id] = link;
    
    // Add forward edge
    EdgeRef fwd{link.id, link.dst, link.cost};
    if (link.src >= adjacency_list_.size()) adjacency_list_.resize(link.src + 1);
    adjacency_list_[link.src].push_back(fwd);
    
    // Add reverse edge (since each physical link is bi-directional as per spec, or we model two directed edges with the same link id)
    // Wait, the spec says "Model each physical link as two directed edges sharing a LinkId". 
    EdgeRef rev{link.id, link.src, link.cost};
    if (link.dst >= adjacency_list_.size()) adjacency_list_.resize(link.dst + 1);
    adjacency_list_[link.dst].push_back(rev);
}

Link* Graph::get_link(LinkId id) {
    auto it = links_.find(id);
    if (it != links_.end()) return &it->second;
    return nullptr;
}

const Link* Graph::get_link(LinkId id) const {
    auto it = links_.find(id);
    if (it != links_.end()) return &it->second;
    return nullptr;
}

void Graph::set_link_status(LinkId id, bool up) {
    auto it = links_.find(id);
    if (it != links_.end()) {
        it->second.up = up;
    }
}

void Graph::update_utilization(LinkId id, double utilization) {
    if (utilization < 0 || utilization > 1) throw std::invalid_argument("utilization must be between 0 and 1");
    auto it = links_.find(id);
    if (it != links_.end()) {
        it->second.utilization = utilization;
    }
}

void Graph::update_link(LinkId id, std::optional<double> bandwidth_mbps,
                        std::optional<double> latency_ms, std::optional<double> cost,
                        std::optional<double> utilization) {
    Link* link = get_link(id);
    if (!link) throw std::invalid_argument("unknown link id");
    if (bandwidth_mbps) { if (*bandwidth_mbps < 0) throw std::invalid_argument("bandwidth must be non-negative"); link->bandwidth_mbps = *bandwidth_mbps; }
    if (latency_ms) { if (*latency_ms < 0) throw std::invalid_argument("latency must be non-negative"); link->latency_ms = *latency_ms; }
    if (cost) { if (*cost < 0) throw std::invalid_argument("cost must be non-negative"); link->cost = *cost; }
    if (utilization) update_utilization(id, *utilization);
}

void Graph::clear() {
    name_to_id_.clear(); id_to_name_.clear(); links_.clear(); adjacency_list_.clear(); next_node_id_ = 0;
}

const std::vector<EdgeRef>& Graph::get_out_edges(NodeId node) const {
    static const std::vector<EdgeRef> empty;
    if (node < adjacency_list_.size()) {
        return adjacency_list_[node];
    }
    return empty;
}

void Graph::load_from_json(const std::string& json_str) {
    auto j = nlohmann::json::parse(json_str);
    if (!j.is_object() || !j.contains("nodes") || !j["nodes"].is_array() ||
        !j.contains("links") || !j["links"].is_array()) {
        throw std::invalid_argument("topology must contain nodes and links arrays");
    }
    clear();
    
    if (j.contains("nodes")) {
        for (const auto& node : j["nodes"]) {
            add_node(node["name"].get<std::string>());
        }
    }
    
    if (j.contains("links")) {
        for (const auto& link_json : j["links"]) {
            Link link;
            link.id = link_json["id"].get<LinkId>();
            
            auto src_opt = get_node_id(link_json["src"].get<std::string>());
            auto dst_opt = get_node_id(link_json["dst"].get<std::string>());
            
            if (src_opt && dst_opt) {
                link.src = *src_opt;
                link.dst = *dst_opt;
                link.bandwidth_mbps = link_json["bandwidth_mbps"].get<double>();
                link.latency_ms = link_json["latency_ms"].get<double>();
                link.cost = link_json["cost"].get<double>();
                if (link_json.contains("utilization")) {
                    link.utilization = link_json["utilization"].get<double>();
                }
                if (link_json.contains("up")) link.up = link_json["up"].get<bool>();
                
                add_link(link);
            } else {
                throw std::invalid_argument("link references an unknown node name");
            }
        }
    }
}

} // namespace core
} // namespace routeforge
