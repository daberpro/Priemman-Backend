#pragma once

#include <src/handlers/user/authenticated_handler_base.hpp>
#include <src/database/cache/user_cache_lru.hpp>
#include <src/component/DynamicConfig/DynamicConfigComponent.hpp>

namespace priemman::handlers::user {

class BasicInfoHandler final : public AuthenticatedHandlerBase {
public:
    static constexpr std::string_view kName = "handler-basic-info";

    BasicInfoHandler(
        [[maybe_unused]] const userver::components::ComponentConfig& config,
        [[maybe_unused]] const userver::components::ComponentContext& context
    ) : AuthenticatedHandlerBase(config,context), 
    _user_cache{
        context.FindComponent<
            cache::user::UserCache
        >().GetCache()
    },
    _dynamic_config{
        &context.FindComponent<priemman::components::DynamicConfigComponent>()
    }
    {
        _dynamic_config->Register("admin-notify-change", {"user_ids"});
    };

    std::string HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;

private:
    mutable userver::cache::LruCacheWrapper<cache::user::UserId, cache::user::UserData> _user_cache;
    priemman::components::DynamicConfigComponent* _dynamic_config;
};

}  // namespace priemman::handlers::user
