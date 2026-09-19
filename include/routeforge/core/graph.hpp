#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace routeforge {
namespace core {

using NodeId = uint32_t;
using LinkId = uint32_t;

struct Link {
    LinkId id;
    NodeId src;
    NodeId dst;
    double bandwidth_mbps;
    double latency_ms;
    double cost;
    double utilization = 0.0; // 0.0 to 1.0
    bool up = true;
};

struct EdgeRef {
    LinkId link_id;
    NodeId to;
    double weight; // Can be cost, latency, etc. depending on strategy
};

class Graph {
public:
    Graph() = default;

    NodeId add_node(const std::string& name);
    std::optional<NodeId> get_node_id(const std::string& name) const;
    std::string get_node_name(NodeId id) const;
    
    void add_link(const Link& link);
    Link* get_link(LinkId id);
    const Link* get_link(LinkId id) const;
    
    void set_link_status(LinkId id, bool up);
    void update_utilization(LinkId id, double utilization);
    void update_link(LinkId id, std::optional<double> bandwidth_mbps,
                     std::optional<double> latency_ms, std::optional<double> cost,
                     std::optional<double> utilization);

    const std::vector<EdgeRef>& get_out_edges(NodeId node) const;
    
    size_t node_count() const { return name_to_id_.size(); }
    const std::unordered_map<LinkId, Link>& links() const { return links_; }
    const std::unordered_map<NodeId, std::string>& nodes() const { return id_to_name_; }
    void clear();

    void load_from_json(const std::string& json_str);

private:
    std::unordered_map<std::string, NodeId> name_to_id_;
    std::unordered_map<NodeId, std::string> id_to_name_;
    
    std::unordered_map<LinkId, Link> links_;
    std::vector<std::vector<EdgeRef>> adjacency_list_;
    
    NodeId next_node_id_ = 0;
};

} // namespace core
} // namespace routeforge
