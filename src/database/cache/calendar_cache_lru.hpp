#pragma once

#include <string>
#include <string_view>

#include <userver/cache/lru_cache_component_base.hpp>
#include <userver/clients/http/client.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/mysql/component.hpp>
#include <userver/yaml_config/schema.hpp>

#include <src/database/user_repository.hpp>

namespace priemman::cache::calendar {

using UserId = std::string;
using CalendarData = std::string;

class CalendarCache final
    : public userver::cache::LruCacheComponent<UserId, CalendarData> {
public:
    static constexpr std::string_view kName{"calendar-cache"};

    CalendarCache(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );

    static userver::yaml_config::Schema GetStaticConfigSchema();

private:
    CalendarData DoGetByKey(const UserId& user_id) override;

    std::shared_ptr<userver::storages::mysql::Cluster> _mysql_cluster;
    database::UserRepository _users;
    userver::clients::http::Client* _client;
    std::string _workspace_host;
    std::string _username;
    std::string _password;
};

}  // namespace priemman::cache::calendar
