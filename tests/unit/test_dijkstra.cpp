#include <gtest/gtest.h>
#include "routeforge/core/graph.hpp"
#include "routeforge/core/dijkstra.hpp"
#include "routeforge/core/failure.hpp"

using namespace routeforge::core;

class DijkstraTest : public ::testing::Test {
protected:
    void SetUp() override {
        a = g.add_node("A");
        b = g.add_node("B");
        c = g.add_node("C");
        d = g.add_node("D");

        // A - B (cost 1)
        g.add_link({1, a, b, 100, 10, 1.0});
        // B - C (cost 2)
        g.add_link({2, b, c, 100, 10, 2.0});
        // A - C (cost 4)
        g.add_link({3, a, c, 100, 10, 4.0});
        // C - D (cost 1)
        g.add_link({4, c, d, 100, 10, 1.0});
    }

    Graph g;
    NodeId a, b, c, d;
};

TEST_F(DijkstraTest, ShortestPath) {
    Dijkstra dijkstra(g);
    CostWeight cost_weight;

    auto path_opt = dijkstra.compute_path(a, c, cost_weight);
    ASSERT_TRUE(path_opt.has_value());
    
    // Path should be A -> B -> C (cost 1 + 2 = 3), rather than A -> C (cost 4)
    EXPECT_EQ(path_opt->total_weight, 3.0);
    EXPECT_EQ(path_opt->nodes.size(), 3);
    EXPECT_EQ(path_opt->nodes[0], a);
    EXPECT_EQ(path_opt->nodes[1], b);
    EXPECT_EQ(path_opt->nodes[2], c);
    
    EXPECT_EQ(path_opt->hops.size(), 2);
    EXPECT_EQ(path_opt->hops[0], 1);
    EXPECT_EQ(path_opt->hops[1], 2);
}

TEST_F(DijkstraTest, LinkDown) {
    Dijkstra dijkstra(g);
    CostWeight cost_weight;

    g.set_link_status(1, false); // A - B is down

    auto path_opt = dijkstra.compute_path(a, c, cost_weight);
    ASSERT_TRUE(path_opt.has_value());
    
    // Path must be A -> C (cost 4)
    EXPECT_EQ(path_opt->total_weight, 4.0);
    EXPECT_EQ(path_opt->nodes.size(), 2);
    EXPECT_EQ(path_opt->hops[0], 3);
}

TEST_F(DijkstraTest, Disconnected) {
    Graph empty_g;
    NodeId n1 = empty_g.add_node("N1");
    NodeId n2 = empty_g.add_node("N2");

    Dijkstra dijkstra(empty_g);
    CostWeight cost_weight;

    auto path_opt = dijkstra.compute_path(n1, n2, cost_weight);
    EXPECT_FALSE(path_opt.has_value());
}

TEST_F(DijkstraTest, Constraints) {
    Dijkstra dijkstra(g);
    CostWeight cost_weight;

    // Constraint: skip links with cost >= 2.0 (so B-C is skipped)
    auto constraint = [](const Link& link) {
        return link.cost < 2.0;
    };

    auto path_opt = dijkstra.compute_path(a, d, cost_weight, constraint);
    // B-C is skipped, A-C is skipped, so A-D is impossible.
    EXPECT_FALSE(path_opt.has_value());
}

TEST_F(DijkstraTest, ReroutesStoredPathAfterLinkFailure) {
    PathRegistry registry;
    Dijkstra dijkstra(g);
    CostWeight weight;
    auto initial = dijkstra.compute_path(a, c, weight);
    ASSERT_TRUE(initial.has_value());

    PathRecord record{};
    record.src = a;
    record.dst = c;
    record.metric = "cost";
    record.current_path = *initial;
    const auto id = registry.add_path(record);

    FailureManager failures(g, registry);
    failures.fail_link(2);

    const auto* rerouted = registry.get_path(id);
    ASSERT_NE(rerouted, nullptr);
    EXPECT_EQ(rerouted->status, PathStatus::REROUTED);
    EXPECT_EQ(rerouted->current_path.hops, std::vector<LinkId>({3}));
    EXPECT_EQ(registry.history(id).size(), 2U);
}

TEST_F(DijkstraTest, MarksPathUnroutableWhenNoBackupExists) {
    PathRegistry registry;
    Dijkstra dijkstra(g);
    CostWeight weight;
    auto initial = dijkstra.compute_path(a, c, weight);
    ASSERT_TRUE(initial.has_value());
    PathRecord record{};
    record.src = a; record.dst = c; record.metric = "cost"; record.current_path = *initial;
    const auto id = registry.add_path(record);
    FailureManager failures(g, registry);
    failures.fail_link(2);
    failures.fail_link(3);
    const auto* result = registry.get_path(id);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->status, PathStatus::UNROUTABLE);
    EXPECT_TRUE(result->current_path.hops.empty());
}
