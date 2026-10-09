#pragma once

#include <string_view>
#include <userver/server/handlers/http_handler_json_base.hpp>
#include "../components/config_map.hpp"

namespace priemman::handlers::config {

class RegisterHandler final
    : public userver::server::handlers::HttpHandlerJsonBase {
public:
    static constexpr std::string_view kName = "handler-register";

    RegisterHandler(
        const userver::components::ComponentConfig& _config,
        const userver::components::ComponentContext& _context
    );

    userver::formats::json::Value HandleRequestJsonThrow(
        const userver::server::http::HttpRequest& _request,
        const userver::formats::json::Value& _json,
        userver::server::request::RequestContext& _context
    ) const override;

private:
    priemman::components::dynamic_config::server::ConfigMapComponent& _config_map;
};

}  // namespace priemman::handlers::config

