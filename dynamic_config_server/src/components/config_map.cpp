#include "config_map.hpp"

#include <stdexcept>
#include <string>
#include <utility>

#include <userver/formats/common/type.hpp>
#include <userver/formats/json/value_builder.hpp>

namespace priemman::components::dynamic_config::server {

ConfigMapComponent::ConfigMapComponent(
    const userver::components::ComponentConfig& _config,
    const userver::components::ComponentContext& _context
)
    : ComponentBase(_config, _context) {}

void ConfigMapComponent::Register(
    std::string_view _service,
    const std::vector<std::string>& _ids
) {
    if (_service.empty()) {
        throw std::invalid_argument("Service name cannot be empty");
    }

    auto _configs_ptr = _configs.StartWrite();
    auto& _service_configs = _configs_ptr->configs[std::string{_service}];

    for (const auto& _id : _ids) {
        if (_id.empty()) {
            throw std::invalid_argument("Config ID cannot be empty");
        }

        if (!_service_configs.contains(_id)) {
            userver::formats::json::ValueBuilder _default_value(
                userver::formats::common::Type::kObject
            );

            _service_configs.emplace(
                _id,
                _default_value.ExtractValue()
            );
        }
    }

    _configs_ptr->update_at = std::chrono::system_clock::now();
    _configs_ptr.Commit();
}

void ConfigMapComponent::Update(
    std::string_view _service,
    std::string_view _id,
    userver::formats::json::Value _value
) {
    auto _configs_ptr = _configs.StartWrite();

    const auto _service_it =
        _configs_ptr->configs.find(std::string{_service});

    if (_service_it == _configs_ptr->configs.end()) {
        throw std::invalid_argument("Unknown dynamic config service");
    }

    const auto _config_it =
        _service_it->second.find(std::string{_id});

    if (_config_it == _service_it->second.end()) {
        throw std::invalid_argument("Unknown dynamic config ID");
    }

    _config_it->second = std::move(_value);
    _configs_ptr->update_at = std::chrono::system_clock::now();
    _configs_ptr.Commit();
}

}  // namespace priemman::components::dynamic_config::server
