#include <gtest/gtest.h>
#include "routeforge/core/graph.hpp"

using namespace routeforge::core;

TEST(GraphTest, AddNode) {
    Graph g;
    NodeId id1 = g.add_node("A");
    NodeId id2 = g.add_node("B");
    NodeId id3 = g.add_node("A"); // Duplicate

    EXPECT_EQ(id1, 0);
    EXPECT_EQ(id2, 1);
    EXPECT_EQ(id1, id3); // Should return same ID for same name
    
    EXPECT_EQ(g.node_count(), 2);
    EXPECT_EQ(g.get_node_name(id1), "A");
    EXPECT_EQ(g.get_node_name(id2), "B");
}

TEST(GraphTest, AddLink) {
    Graph g;
    NodeId a = g.add_node("A");
    NodeId b = g.add_node("B");

    Link link{100, a, b, 10.0, 5.0, 1.0, 0.0, true};
    g.add_link(link);

    const Link* fetched = g.get_link(100);
    ASSERT_NE(fetched, nullptr);
    EXPECT_EQ(fetched->id, 100);
    EXPECT_EQ(fetched->src, a);
    EXPECT_EQ(fetched->dst, b);

    // Out edges
    auto edgesA = g.get_out_edges(a);
    EXPECT_EQ(edgesA.size(), 1);
    EXPECT_EQ(edgesA[0].to, b);

    // Undirected nature in our model: we add reverse edge too
    auto edgesB = g.get_out_edges(b);
    EXPECT_EQ(edgesB.size(), 1);
    EXPECT_EQ(edgesB[0].to, a);
}

TEST(GraphTest, LoadFromJson) {
    std::string json_data = R"({
        "nodes": [{"name": "A"}, {"name": "B"}],
        "links": [
            {"id": 1, "src": "A", "dst": "B", "bandwidth_mbps": 100, "latency_ms": 10, "cost": 5}
        ]
    })";

    Graph g;
    g.load_from_json(json_data);

    EXPECT_EQ(g.node_count(), 2);
    const Link* link = g.get_link(1);
    ASSERT_NE(link, nullptr);
    EXPECT_EQ(link->latency_ms, 10);
}
