#include "DynamicConfigComponent.hpp"

#include <userver/clients/http/client.hpp>
#include <userver/components/component_context.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/formats/common/items.hpp>
#include <userver/formats/common/type.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/formats/serialize/common_containers.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/http/http_method.hpp>
#include <userver/utils/datetime.hpp>
#include <userver/yaml_config/merge_schemas.hpp>
#include <userver/components/component_config.hpp>

namespace priemman::components {

DynamicConfigComponent::DynamicConfigComponent(
    const userver::components::ComponentConfig& _config,
    const userver::components::ComponentContext& _context
)
    : ComponentBase(_config, _context),
      _http_client(
          _context.FindComponent<userver::components::HttpClient>().GetHttpClient()
      ),
      _server_url(_config["url"].As<std::string>()),
      _request_timeout(
          _config["http-timeout"].As<std::chrono::milliseconds>(
              std::chrono::seconds(5)
          )
      ),
      _update_interval(_config["update-time"].As<std::chrono::milliseconds>()),
      _updater(
          "dynamic_config_updater",
          _update_interval,
          [this] { Poll(); }
      ) {
}

DynamicConfigComponent::~DynamicConfigComponent() = default;

bool DynamicConfigComponent::Register(
    std::string_view _service,
    const std::vector<std::string>& _ids
) {
    {
        auto state = _services.StartWrite();
        if (state->contains(std::string{_service})) {
            return true;
        }
        ServiceState new_state;
        new_state.ids = _ids;
        new_state.configs = userver::formats::json::ValueBuilder{userver::formats::common::Type::kObject}.ExtractValue();
        new_state.updated_at = "";
        state->emplace(std::string{_service}, std::move(new_state));
        state.Commit();
    }
    
    return FetchInitial(_service, _ids);
}

bool DynamicConfigComponent::Update(
    std::string_view _service,
    const userver::formats::json::Value& _configs
) {
    userver::formats::json::ValueBuilder body;
    body["service"] = _service;
    body["configs"] = _configs;

    auto response = Request("PUT", "/configs", body.ExtractValue());
    return response.has_value();
}

std::optional<userver::formats::json::Value> DynamicConfigComponent::Get(
    std::string_view _service,
    const std::vector<std::string>& _ids,
    std::string_view /*_updated_since*/
) {
    // 1. Cek cache lokal
    {
        auto state = _services.Read();
        auto it = state->find(std::string{_service});
        if (it != state->end() && !it->second.configs.IsEmpty()) {
            return it->second.configs;
        }
    }

    // 2. Fallback ke server jika cache kosong
    std::vector<std::string> ids_to_fetch = _ids;
    if (ids_to_fetch.empty()) {
        auto state = _services.Read();
        auto it = state->find(std::string{_service});
        if (it != state->end()) {
            ids_to_fetch = it->second.ids;
        }
    }

    if (FetchInitial(_service, ids_to_fetch)) {
        auto state = _services.Read();
        auto it = state->find(std::string{_service});
        if (it != state->end() && !it->second.configs.IsEmpty()) {
            return it->second.configs;
        }
    }
    
    return std::nullopt;
}

void DynamicConfigComponent::OnAllComponentsLoaded() {
    // PeriodicTask sudah otomatis berjalan
}

void DynamicConfigComponent::OnAllComponentsAreStopping() {
    _updater.Stop();
}

userver::yaml_config::Schema DynamicConfigComponent::GetStaticConfigSchema() {
    using userver::yaml_config::MergeSchemas;
    using userver::components::ComponentBase;
    return MergeSchemas<ComponentBase>(R"(
type: object
description: Dynamic Config Client Component
additionalProperties: false
properties:
    url:
        type: string
        description: URL of the dynamic config server
    update-time:
        type: string
        description: Interval to poll the server (e.g., 2s)
    http-timeout:
        type: string
        description: HTTP request timeout
        defaultDescription: 5s
)");
}

std::optional<userver::formats::json::Value> DynamicConfigComponent::Request(
    std::string_view _method,
    std::string_view _path,
    const userver::formats::json::Value& _body
) {
    try {
        auto method = userver::clients::http::HttpMethod::kGet;
        if (_method == "POST") method = userver::clients::http::HttpMethod::kPost;
        else if (_method == "PUT") method = userver::clients::http::HttpMethod::kPut;

        auto request = _http_client.CreateRequest()
            .method(method)
            .url(_server_url + std::string{_path})
            .timeout(_request_timeout)
            .headers({{"Content-Type", "application/json"}});

        if (!_body.IsEmpty()) {
            request.data(userver::formats::json::ToString(_body));
        }

        auto response = request.perform();

        if (response->IsOk()) {
            if (!response->body().empty()) {
                return userver::formats::json::FromString(response->body());
            }
            return userver::formats::json::ValueBuilder{userver::formats::common::Type::kObject}.ExtractValue();
        }
        LOG_ERROR() << "HTTP request failed with status: "
                    << response->status_code();
    } catch (const std::exception& e) {
        LOG_ERROR() << "HTTP request exception: " << e.what();
    }
    return std::nullopt;
}

bool DynamicConfigComponent::FetchInitial(
    std::string_view _service,
    const std::vector<std::string>& _ids
) {
    userver::formats::json::ValueBuilder body;
    body["service"] = _service;
    body["ids"] = _ids;
    body["updated_since"] = "";

    auto response = Request("POST", "/register", body.ExtractValue());
    if (!response) {
        return false;
    }

    auto state = _services.StartWrite();
    auto it = state->find(std::string{_service});
    if (it != state->end()) {
        it->second.configs = *response;
        it->second.updated_at =
            userver::utils::datetime::Timestring(std::chrono::system_clock::now());
    }
    state.Commit();
    return true;
}

void DynamicConfigComponent::Poll() {
    std::vector<std::string> services_to_refresh;
    {
        auto state = _services.Read();
        for (const auto& [service, _] : *state) {
            services_to_refresh.emplace_back(service);
        }
    }

    for (const auto& service : services_to_refresh) {
        RefreshService(service);
    }
}

void DynamicConfigComponent::RefreshService(std::string_view _service) {
    std::string updated_since;
    std::vector<std::string> ids;
    {
        auto state = _services.Read();
        auto it = state->find(std::string{_service});
        if (it != state->end()) {
            updated_since = it->second.updated_at;
            ids = it->second.ids;
        } else {
            return;
        }
    }

    userver::formats::json::ValueBuilder body;
    body["service"] = _service;
    body["ids"] = ids;
    body["updated_since"] = updated_since;

    auto response = Request("POST", "/configs", body.ExtractValue());
    if (!response || response->IsEmpty()) {
        return;
    }

    auto state = _services.StartWrite();
    auto it = state->find(std::string{_service});
    if (it != state->end()) {
        userver::formats::json::ValueBuilder builder(it->second.configs);
        
        for (const auto& [key, value] : userver::formats::common::Items(*response)) {
            builder[key] = value;
        }
        
        // Extract kembali menjadi Value yang immutable
        it->second.configs = builder.ExtractValue();
        it->second.updated_at = userver::utils::datetime::Timestring(std::chrono::system_clock::now());
    }
    state.Commit();
}

}  // namespace priemman::component