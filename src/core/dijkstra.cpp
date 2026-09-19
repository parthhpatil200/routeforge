#include "routeforge/core/dijkstra.hpp"
#include <queue>
#include <limits>
#include <algorithm>

namespace routeforge {
namespace core {

struct State {
    NodeId node;
    double dist;
    
    bool operator>(const State& other) const {
        return dist > other.dist;
    }
};

std::optional<Path> Dijkstra::compute_path(NodeId src, NodeId dst, const WeightStrategy& weight_strategy, 
                                           std::function<bool(const Link&)> constraint) const {
    size_t num_nodes = graph_.node_count();
    if (src >= num_nodes || dst >= num_nodes) return std::nullopt;
    if (src == dst) {
        Path p;
        p.nodes.push_back(src);
        p.total_weight = 0.0;
        return p;
    }

    std::vector<double> dist(num_nodes, std::numeric_limits<double>::infinity());
    std::vector<NodeId> parent_node(num_nodes, std::numeric_limits<NodeId>::max());
    std::vector<LinkId> parent_link(num_nodes, std::numeric_limits<LinkId>::max());
    
    std::priority_queue<State, std::vector<State>, std::greater<State>> pq;
    
    dist[src] = 0.0;
    pq.push({src, 0.0});
    
    while (!pq.empty()) {
        auto [u, d] = pq.top();
        pq.pop();
        
        if (d > dist[u]) continue;
        if (u == dst) break;
        
        for (const auto& edge : graph_.get_out_edges(u)) {
            const Link* link = graph_.get_link(edge.link_id);
            if (!link || !link->up) continue;
            
            if (constraint && !constraint(*link)) continue;
            
            double w = weight_strategy.get_weight(*link);
            if (dist[u] + w < dist[edge.to]) {
                dist[edge.to] = dist[u] + w;
                parent_node[edge.to] = u;
                parent_link[edge.to] = edge.link_id;
                pq.push({edge.to, dist[edge.to]});
            }
        }
    }
    
    if (dist[dst] == std::numeric_limits<double>::infinity()) {
        return std::nullopt;
    }
    
    Path path;
    path.total_weight = dist[dst];
    
    NodeId curr = dst;
    while (curr != src) {
        path.nodes.push_back(curr);
        path.hops.push_back(parent_link[curr]);
        curr = parent_node[curr];
    }
    path.nodes.push_back(src);
    
    std::reverse(path.nodes.begin(), path.nodes.end());
    std::reverse(path.hops.begin(), path.hops.end());
    
    return path;
}

} // namespace core
} // namespace routeforge
