#include "register.hpp"
#include <string>
#include <vector>

#include <userver/components/component_context.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/server/http/http_status.hpp>

#include "../components/config_map.hpp"

namespace priemman::handlers::config {

RegisterHandler::RegisterHandler(
    const userver::components::ComponentConfig& _config,
    const userver::components::ComponentContext& _context
)
    : HttpHandlerJsonBase(_config, _context),
      _config_map(
          _context.FindComponent<
              priemman::components::dynamic_config::server::ConfigMapComponent
          >()
      ) {}

userver::formats::json::Value RegisterHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& _request,
    const userver::formats::json::Value& _json,
    [[maybe_unused]] userver::server::request::RequestContext& _context
) const {
    
    auto& _response = _request.GetHttpResponse();

    try {
        const auto _service = _json["service"].As<std::string>();
        const auto _ids = _json["ids"].As<std::vector<std::string>>();

        if (_service.empty()) {
            _response.SetStatus(
                userver::server::http::HttpStatus::BadRequest
            );
            return {};
        }

        _config_map.Register(_service, _ids);

        _response.SetStatus(
            userver::server::http::HttpStatus::kOk
        );
        return {};
    } catch (const std::exception&) {
        _response.SetStatus(
            userver::server::http::HttpStatus::BadRequest
        );
        return {};
    }
}

}  // namespace priemman::handlers::config