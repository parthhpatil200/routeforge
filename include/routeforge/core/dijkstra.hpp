#pragma once

#include "routeforge/core/graph.hpp"
#include <vector>
#include <optional>
#include <functional>

namespace routeforge {
namespace core {

struct Path {
    std::vector<LinkId> hops;
    std::vector<NodeId> nodes;
    double total_weight = 0.0;
};

// WeightStrategy interface
class WeightStrategy {
public:
    virtual ~WeightStrategy() = default;
    virtual double get_weight(const Link& link) const = 0;
};

class CostWeight : public WeightStrategy {
public:
    double get_weight(const Link& link) const override { return link.cost; }
};

class LatencyWeight : public WeightStrategy {
public:
    double get_weight(const Link& link) const override { return link.latency_ms; }
};

class HopWeight : public WeightStrategy {
public:
    double get_weight(const Link& link) const override { return 1.0; }
};

class Dijkstra {
public:
    Dijkstra(const Graph& graph) : graph_(graph) {}

    // Computes shortest path from src to dst.
    // Constraints function can be provided to filter out edges (e.g., if false, skip edge)
    std::optional<Path> compute_path(NodeId src, NodeId dst, const WeightStrategy& weight_strategy, 
                                     std::function<bool(const Link&)> constraint = nullptr) const;

private:
    const Graph& graph_;
};

} // namespace core
} // namespace routeforge
