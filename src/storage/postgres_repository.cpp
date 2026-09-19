#include "routeforge/storage/postgres_repository.hpp"

#include <nlohmann/json.hpp>
#include <pqxx/pqxx>

namespace routeforge::storage {
namespace {

const char* status(core::PathStatus value) {
    return value == core::PathStatus::ACTIVE ? "ACTIVE" :
           value == core::PathStatus::REROUTED ? "REROUTED" : "UNROUTABLE";
}

pqxx::connection open(const std::string& connection_string) {
    pqxx::connection connection{connection_string};
    if (!connection.is_open()) throw std::runtime_error("could not connect to PostgreSQL");
    return connection;
}
}

PostgresRepository::PostgresRepository(std::string connection_string)
    : connection_string_(std::move(connection_string)) {
    auto connection = open(connection_string_);
}

std::optional<std::string> PostgresRepository::load_topology_json() const {
    auto connection = open(connection_string_);
    pqxx::read_transaction tx{connection};
    const auto nodes = tx.exec("SELECT name FROM nodes ORDER BY id");
    if (nodes.empty()) return std::nullopt;

    nlohmann::json result;
    result["nodes"] = nlohmann::json::array();
    result["links"] = nlohmann::json::array();
    for (const auto& row : nodes) result["nodes"].push_back({{"name", row[0].as<std::string>()}});
    const auto links = tx.exec(
        "SELECT l.id, src.name, dst.name, l.bandwidth_mbps, l.latency_ms, l.cost, l.utilization, l.status "
        "FROM links l JOIN nodes src ON src.id=l.src JOIN nodes dst ON dst.id=l.dst ORDER BY l.id");
    for (const auto& row : links) {
        result["links"].push_back({{"id", row[0].as<core::LinkId>()}, {"src", row[1].as<std::string>()},
            {"dst", row[2].as<std::string>()}, {"bandwidth_mbps", row[3].as<double>()},
            {"latency_ms", row[4].as<double>()}, {"cost", row[5].as<double>()},
            {"utilization", row[6].as<double>()}, {"up", row[7].as<std::string>() == "UP"}});
    }
    return result.dump();
}

void PostgresRepository::save_topology(const core::Graph& graph) const {
    auto connection = open(connection_string_);
    pqxx::work tx{connection};
    tx.exec("TRUNCATE nodes CASCADE");
    for (const auto& [id, name] : graph.nodes()) {
        tx.exec_params("INSERT INTO nodes (name) VALUES ($1)", name);
    }
    for (const auto& [id, link] : graph.links()) {
        tx.exec_params("INSERT INTO links (id, src, dst, bandwidth_mbps, latency_ms, cost, utilization, status) "
                       "VALUES ($1, (SELECT id FROM nodes WHERE name=$2), (SELECT id FROM nodes WHERE name=$3), "
                       "$4, $5, $6, $7, $8)",
                       link.id, graph.get_node_name(link.src), graph.get_node_name(link.dst), link.bandwidth_mbps,
                       link.latency_ms, link.cost, link.utilization, link.up ? "UP" : "DOWN");
    }
    tx.commit();
}

void PostgresRepository::save_link(const core::Link& link) const {
    auto connection = open(connection_string_);
    pqxx::work tx{connection};
    tx.exec_params("UPDATE links SET bandwidth_mbps=$2, latency_ms=$3, cost=$4, utilization=$5, status=$6 WHERE id=$1",
                   link.id, link.bandwidth_mbps, link.latency_ms, link.cost, link.utilization,
                   link.up ? "UP" : "DOWN");
    tx.commit();
}

void PostgresRepository::save_path(const core::PathRecord& record, const core::Graph& graph) const {
    auto connection = open(connection_string_);
    pqxx::work tx{connection};
    nlohmann::json constraints = {{"max_utilization", record.max_utilization},
                                  {"min_available_mbps", record.min_available_mbps},
                                  {"exclude_links", record.excluded_links}};
    tx.exec_params("INSERT INTO paths (id, source, target, metric, constraints, status, total_cost) VALUES "
                   "($1, (SELECT id FROM nodes WHERE name=$2), (SELECT id FROM nodes WHERE name=$3), $4, $5::jsonb, $6, $7) "
                   "ON CONFLICT (id) DO UPDATE SET source=EXCLUDED.source, target=EXCLUDED.target, metric=EXCLUDED.metric, "
                   "constraints=EXCLUDED.constraints, status=EXCLUDED.status, total_cost=EXCLUDED.total_cost",
                   record.id, graph.get_node_name(record.src), graph.get_node_name(record.dst), record.metric,
                   constraints.dump(), status(record.status), record.current_path.total_weight);
    tx.exec_params("DELETE FROM path_hops WHERE path_id=$1", record.id);
    for (std::size_t sequence = 0; sequence < record.current_path.hops.size(); ++sequence) {
        tx.exec_params("INSERT INTO path_hops (path_id, seq, link_id) VALUES ($1, $2, $3)",
                       record.id, sequence, record.current_path.hops[sequence]);
    }
    tx.exec_params("INSERT INTO path_history (path_id, status, total_cost) VALUES ($1, $2, $3)",
                   record.id, status(record.status), record.current_path.total_weight);
    tx.commit();
}

void PostgresRepository::save_link_event(core::LinkId link_id, const std::string& event) const {
    auto connection = open(connection_string_);
    pqxx::work tx{connection};
    tx.exec_params("INSERT INTO link_events (link_id, event) VALUES ($1, $2)", link_id, event);
    tx.commit();
}

}  // namespace routeforge::storage
