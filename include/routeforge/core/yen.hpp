#pragma once

#include "routeforge/core/graph.hpp"
#include "routeforge/core/dijkstra.hpp"
#include <vector>

namespace routeforge {
namespace core {

class Yen {
public:
    Yen(Graph& graph) : graph_(graph) {}

    std::vector<Path> compute_k_shortest_paths(NodeId src, NodeId dst, 
                                               const WeightStrategy& weight_strategy,
                                               size_t k,
                                               std::function<bool(const Link&)> base_constraint = nullptr);

private:
    Graph& graph_;
};

} // namespace core
} // namespace routeforge
