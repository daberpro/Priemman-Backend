#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <userver/components/component_base.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/rcu/rcu.hpp>

namespace priemman::components::dynamic_config::server {

struct ConfigMap {
    using ServiceConfigs = std::unordered_map<
        std::string,
        userver::formats::json::Value
    >;

    std::chrono::system_clock::time_point update_at {};
    std::unordered_map<std::string, ServiceConfigs> configs;
};

class ConfigMapComponent final
    : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName = "config-map";

    ConfigMapComponent(
        const userver::components::ComponentConfig& _config,
        const userver::components::ComponentContext& _context
    );

    void Register(
        std::string_view _service,
        const std::vector<std::string>& _ids
    );

    void Update(
        std::string_view _service,
        std::string_view _id,
        userver::formats::json::Value _value
    );

    auto Read() const {
        return _configs.Read();
    }

private:
    userver::rcu::Variable<ConfigMap> _configs;
};

}  // namespace priemman::components::dynamic_config::server
