#include "config.hpp"
#include <format>
#include <string>
#include <vector>

#include <userver/formats/common/items.hpp>
#include <userver/formats/parse/to.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_method.hpp>
#include <userver/server/http/http_status.hpp>
#include <userver/utils/datetime.hpp>
#include <userver/logging/log.hpp>

namespace priemman::handlers::config {

struct DynamicConfigGetRequest {
    std::string service;
    std::vector<std::string> ids;
    std::string updated_since;
};

std::vector<std::string> Parse(
    const userver::formats::json::Value& _json,
    userver::formats::parse::To<std::vector<std::string>>
) {
    std::vector<std::string> _result;
    _result.reserve(_json.GetSize());

    for (const auto& _item : _json) {
        _result.emplace_back(_item.As<std::string>());
    }

    return _result;
}

DynamicConfigGetRequest Parse(
    const userver::formats::json::Value& _json,
    userver::formats::parse::To<DynamicConfigGetRequest>
) {
    return {
        .service = _json["service"].As<std::string>(),
        .ids = _json["ids"].As<std::vector<std::string>>({}),
        .updated_since = _json["updated_since"].As<std::string>("")
    };
}

struct DynamicConfigPostRequest {
    std::string service;
    userver::formats::json::Value configs;
};

DynamicConfigPostRequest Parse(
    const userver::formats::json::Value& _json,
    userver::formats::parse::To<DynamicConfigPostRequest>
) {
    return {
        .service = _json["service"].As<std::string>(),
        .configs = _json["configs"]
    };
}

DynamicConfigHandler::DynamicConfigHandler(
    const userver::components::ComponentConfig& _config,
    const userver::components::ComponentContext& _context
)
:   HttpHandlerJsonBase(_config, _context),
        _config_map(
        _context.FindComponent<
            priemman::components::dynamic_config::server::ConfigMapComponent
        >()
    ) {}

userver::formats::json::Value DynamicConfigHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& _req,
    const userver::formats::json::Value& _val,
    userver::server::request::RequestContext& /*_ctx*/
) const {

auto& _res = _req.GetHttpResponse();

try {
    switch (_req.GetMethod()) {
        case userver::server::http::HttpMethod::kPost: {

            const auto _request = _val.As<DynamicConfigGetRequest>();

            if (_request.service.empty()) {
                _res.SetStatus(
                    userver::server::http::HttpStatus::kBadRequest
                );
                return {};
            }

            const auto _snapshot = _config_map.Read();
            const auto _service_it = _snapshot->configs.find(_request.service);

            if (_service_it == _snapshot->configs.end()) {
                _res.SetStatus(
                    userver::server::http::HttpStatus::kBadRequest
                );

                LOG_ERROR() << std::format(
                    "Dynamic config for service {} not found; "
                    "perhaps the service has not registered yet",
                    _request.service
                );

                return {};
            }

            if (!_request.updated_since.empty() && userver::utils::datetime::Stringtime(_request.updated_since) >= _snapshot->update_at) {
                return {};
            }

            LOG_DEBUG() << std::format(
                "Sending dynamic config for service {}",
                _request.service
            );

            const auto& _service_configs = _service_it->second;
            userver::formats::json::ValueBuilder _result;

            if (_request.ids.empty()) {
                for (const auto& [_id, _value] : _service_configs) {
                    _result[_id] = _value;
                }
            } else {
                for (const auto& _id : _request.ids) {
                    const auto _it = _service_configs.find(_id);

                    if (_it == _service_configs.end()) {
                        LOG_ERROR() << std::format(
                            "Failed to find config ID {}",
                            _id
                        );
                        continue;
                    }

                    _result[_id] = _it->second;
                }
            }

            return _result.ExtractValue();
        }

        case userver::server::http::HttpMethod::kPut: {
            
            const auto _request = _val.As<DynamicConfigPostRequest>();

            if (_request.service.empty() ||
                !_request.configs.IsObject()) {
                _res.SetStatus(
                    userver::server::http::HttpStatus::kBadRequest
                );
                return {};
            }

            const auto _snapshot = _config_map.Read();
            const auto _service_it = _snapshot->configs.find(_request.service);

            if (_service_it == _snapshot->configs.end()) {
                _res.SetStatus(
                    userver::server::http::HttpStatus::kBadRequest
                );

                LOG_ERROR() << std::format(
                    "Dynamic config for service {} not found; "
                    "perhaps the service has not registered yet",
                    _request.service
                );

                return {};
            }

            for (const auto& [_id, _value] :
                 userver::formats::common::Items(_request.configs)) {
                if (!_service_it->second.contains(_id)) {
                    LOG_ERROR() << std::format(
                        "Failed to find config ID {}",
                        _id
                    );
                    continue;
                }

                _config_map.Update(
                    _request.service,
                    _id,
                    _value
                );
            }

            

            return {};
        }

        case userver::server::http::HttpMethod::kDelete:
        case userver::server::http::HttpMethod::kHead:
        case userver::server::http::HttpMethod::kGet:
        case userver::server::http::HttpMethod::kPatch:
        case userver::server::http::HttpMethod::kConnect:
        case userver::server::http::HttpMethod::kOptions:
        case userver::server::http::HttpMethod::kUnknown:
            break;
    }
} catch (const std::exception& _e) {
    _res.SetStatus(
        userver::server::http::HttpStatus::kBadRequest
    );

    LOG_ERROR() << std::format(
        "Invalid dynamic config request: {}",
        _e.what()
    );

    return {};
}

_res.SetStatus(
    userver::server::http::HttpStatus::kBadRequest
);

return {};

}

}