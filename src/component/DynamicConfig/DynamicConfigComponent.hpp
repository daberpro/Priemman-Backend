#pragma once
#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <userver/clients/http/client.hpp>
#include <userver/components/component_base.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/rcu/rcu.hpp>
#include <userver/utils/periodic_task.hpp>
#include <userver/yaml_config/schema.hpp>

namespace priemman::components {

class DynamicConfigComponent final
: public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName = "dynamic-config-custom";

    DynamicConfigComponent(
        const userver::components::ComponentConfig& _config,
        const userver::components::ComponentContext& _context
    );
    ~DynamicConfigComponent() override;

    bool Register(
        std::string_view _service,
        const std::vector<std::string>& _ids
    );
    bool Update(
        std::string_view _service,
        const userver::formats::json::Value& _configs
    );
    std::optional<userver::formats::json::Value> Get(
        std::string_view _service,
        const std::vector<std::string>& _ids,
        std::string_view _updated_since = {}
    );

    void OnAllComponentsLoaded() override;
    void OnAllComponentsAreStopping() override;

    static userver::yaml_config::Schema GetStaticConfigSchema();

private:
    using Json = userver::formats::json::Value;

    struct ServiceState {
        std::vector<std::string> ids;
        Json configs;
        std::string updated_at;
    };

    std::optional<Json> Request(
        std::string_view _method,
        std::string_view _path,
        const Json& _body
    );
    bool FetchInitial(
        std::string_view _service,
        const std::vector<std::string>& _ids
    );
    void Poll();
    void RefreshService(std::string_view _service);

    userver::clients::http::Client& _http_client;
    const std::string _server_url;
    const std::chrono::milliseconds _request_timeout;
    const std::chrono::milliseconds _update_interval;
    
    // Menggunakan RCU Variable untuk performa baca tinggi dan tulis jarang
    userver::rcu::Variable<std::unordered_map<std::string, ServiceState>> _services;
    
    userver::utils::PeriodicTask _updater;
};

}  // namespace priemman::component