#pragma once

#include <src/database/cache/user_cache_lru.hpp>
#include <src/handlers/user/authenticated_handler_base.hpp>

#include <userver/cache/lru_cache_component_base.hpp>

namespace priemman::handlers::admin {

class AdminUpgradeListHandler final : public AuthenticatedHandlerBase {
public:
    static constexpr std::string_view kName = "handler-admin-upgrades";

    using AuthenticatedHandlerBase::AuthenticatedHandlerBase;

    std::string HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;
};

class AdminUpgradeReviewHandler final : public AuthenticatedHandlerBase {
public:
    static constexpr std::string_view kName = "handler-admin-upgrades-review";

    using AuthenticatedHandlerBase::AuthenticatedHandlerBase;

    std::string HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;
};

class AdminUpgradeConfirmPaymentHandler final : public AuthenticatedHandlerBase {
public:
    static constexpr std::string_view kName = "handler-admin-upgrades-confirm-payment";

    AdminUpgradeConfirmPaymentHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    ) : AuthenticatedHandlerBase(config, context),
        _user_cache{
            context.FindComponent<cache::user::UserCache>().GetCache()
        } {}

    std::string HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;

private:
    mutable userver::cache::LruCacheWrapper<
        cache::user::UserId,
        cache::user::UserData
    > _user_cache;
};

}  // namespace priemman::handlers::admin
