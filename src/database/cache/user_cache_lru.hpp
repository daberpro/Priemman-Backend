#pragma once

#include <userver/cache/lru_cache_component_base.hpp>
#include <userver/components/component_list.hpp>
#include <userver/components/component_context.hpp>
#include <userver/components/component_config.hpp>
#include <src/handlers/user/proto_convert.hpp>
#include <userver/storages/mysql/component.hpp>
#include <proto/user.pb.h>

namespace priemman::cache::user {
    
    using UserId = std::string;
    using UserData = priemman::v1::User;

    class UserCache final : public userver::cache::LruCacheComponent<UserId,UserData> {
    public:

        static constexpr std::string_view kName{"user-cache"};
        
        UserCache(
            [[maybe_unused]] const userver::components::ComponentConfig& config,
            [[maybe_unused]] const userver::components::ComponentContext& context
        ) : userver::cache::LruCacheComponent<UserId, UserData>(config,context), 
        _mysql_cluster{
            context.FindComponent<userver::storages::mysql::Component>("database").GetCluster()
        },
        _users(&_mysql_cluster),
        _accounts(&_mysql_cluster){}

    private:

        UserData DoGetByKey([[maybe_unused]] const UserId& user_id) override;
        std::shared_ptr<userver::storages::mysql::Cluster> _mysql_cluster;
        database::UserRepository _users;
        database::AccountRepository _accounts;

    };

}