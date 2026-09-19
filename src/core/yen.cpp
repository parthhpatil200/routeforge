#include "routeforge/core/yen.hpp"
#include <set>
#include <algorithm>

namespace routeforge {
namespace core {

std::vector<Path> Yen::compute_k_shortest_paths(NodeId src, NodeId dst, 
                                                const WeightStrategy& weight_strategy,
                                                size_t k,
                                                std::function<bool(const Link&)> base_constraint) {
    std::vector<Path> A;
    if (k == 0) return A;

    Dijkstra dijkstra(graph_);
    auto initial_path = dijkstra.compute_path(src, dst, weight_strategy, base_constraint);
    
    if (!initial_path) return A;
    A.push_back(*initial_path);

    std::vector<Path> B; // Potential k-th shortest paths

    for (size_t k_idx = 1; k_idx < k; ++k_idx) {
        const auto& prev_path = A[k_idx - 1];

        for (size_t i = 0; i < prev_path.nodes.size() - 1; ++i) {
            NodeId spur_node = prev_path.nodes[i];
            
            // Build root path
            Path root_path;
            root_path.nodes.assign(prev_path.nodes.begin(), prev_path.nodes.begin() + i + 1);
            root_path.hops.assign(prev_path.hops.begin(), prev_path.hops.begin() + i);
            root_path.total_weight = 0.0;
            for (LinkId hop : root_path.hops) {
                const Link* l = graph_.get_link(hop);
                if (l) root_path.total_weight += weight_strategy.get_weight(*l);
            }

            // Exclude links used by previous paths sharing the same root
            std::set<LinkId> excluded_links;
            for (const auto& p : A) {
                bool match = true;
                if (p.nodes.size() <= i + 1) match = false;
                else {
                    for (size_t j = 0; j <= i; ++j) {
                        if (p.nodes[j] != root_path.nodes[j]) {
                            match = false;
                            break;
                        }
                    }
                }
                
                if (match && i < p.hops.size()) {
                    excluded_links.insert(p.hops[i]);
                }
            }

            auto spur_constraint = [&](const Link& link) {
                if (base_constraint && !base_constraint(link)) return false;
                if (excluded_links.count(link.id)) return false;
                
                // Exclude nodes in root path before spur node to maintain simple path
                for (size_t r = 0; r < i; ++r) {
                    if (link.dst == root_path.nodes[r]) return false;
                }
                
                return true;
            };

            auto spur_path_opt = dijkstra.compute_path(spur_node, dst, weight_strategy, spur_constraint);
            
            if (spur_path_opt) {
                Path total_path = root_path;
                total_path.nodes.insert(total_path.nodes.end(), spur_path_opt->nodes.begin() + 1, spur_path_opt->nodes.end());
                total_path.hops.insert(total_path.hops.end(), spur_path_opt->hops.begin(), spur_path_opt->hops.end());
                total_path.total_weight += spur_path_opt->total_weight;
                
                // Avoid duplicates in B
                bool exists = false;
                for (const auto& bp : B) {
                    if (bp.hops == total_path.hops) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    B.push_back(total_path);
                }
            }
        }

        if (B.empty()) break;

        // Sort B and pick the shortest
        std::sort(B.begin(), B.end(), [](const Path& a, const Path& b) {
            return a.total_weight < b.total_weight;
        });

        A.push_back(B.front());
        B.erase(B.begin());
    }

    return A;
}

} // namespace core
} // namespace routeforge
