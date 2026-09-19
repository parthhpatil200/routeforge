#pragma once

#include "routeforge/core/failure.hpp"
#include "routeforge/core/graph.hpp"

#include <optional>
#include <string>

namespace routeforge::storage {

// Opens short-lived connections per operation. This keeps the service simple
// and lets libpqxx/connection RAII return all resources on every request.
class PostgresRepository {
public:
    explicit PostgresRepository(std::string connection_string);

    std::optional<std::string> load_topology_json() const;
    void save_topology(const core::Graph& graph) const;
    void save_link(const core::Link& link) const;
    void save_path(const core::PathRecord& record, const core::Graph& graph) const;
    void save_link_event(core::LinkId link_id, const std::string& event) const;

private:
    std::string connection_string_;
};

}  // namespace routeforge::storage
