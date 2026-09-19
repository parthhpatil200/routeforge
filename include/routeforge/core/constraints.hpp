#pragma once

#include "routeforge/core/graph.hpp"
#include <functional>
#include <memory>
#include <vector>

namespace routeforge {
namespace core {

class Constraints {
public:
    using Predicate = std::function<bool(const Link&)>;

    Constraints& max_utilization(double limit) {
        predicates_.push_back([limit](const Link& link) {
            return link.utilization <= limit;
        });
        return *this;
    }

    Constraints& min_available_mbps(double mbps) {
        predicates_.push_back([mbps](const Link& link) {
            double available = link.bandwidth_mbps * (1.0 - link.utilization);
            return available >= mbps;
        });
        return *this;
    }

    Constraints& exclude_link(LinkId id) {
        predicates_.push_back([id](const Link& link) {
            return link.id != id;
        });
        return *this;
    }

    Predicate build() const {
        auto preds = predicates_;
        return [preds](const Link& link) {
            for (const auto& p : preds) {
                if (!p(link)) return false;
            }
            return true;
        };
    }

private:
    std::vector<Predicate> predicates_;
};

} // namespace core
} // namespace routeforge
