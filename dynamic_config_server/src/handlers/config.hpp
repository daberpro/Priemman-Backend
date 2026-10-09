#pragma once

#include <string_view>
#include <userver/server/handlers/http_handler_json_base.hpp>
#include <userver/components/component_context.hpp>
#include <userver/components/component_config.hpp>
#include "../components/config_map.hpp"

namespace priemman::handlers::config {

class DynamicConfigHandler final : public userver::server::handlers::HttpHandlerJsonBase {
public:
static constexpr std::string_view kName{"handler-dynamic-config"};

DynamicConfigHandler(
    const userver::components::ComponentConfig& _config,
    const userver::components::ComponentContext& _context
);

userver::formats::json::Value HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& _req,
    const userver::formats::json::Value& _val,
    userver::server::request::RequestContext& _ctx
) const override;

private:
    priemman::components::dynamic_config::server::ConfigMapComponent& _config_map;
};

}
