#pragma once

#include <string_view>
#include <src/database/cache/calendar_cache_lru.hpp>
#include <src/handlers/user/authenticated_handler_base.hpp>

namespace priemman::handlers::workspace {

class CalendarHandler final
    : public AuthenticatedHandlerBase {

public:

    static constexpr std::string_view kName = "handler-calendar-workspace";

    CalendarHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    ) : AuthenticatedHandlerBase(config, context),
        _calendar_cache{
            context.FindComponent<cache::calendar::CalendarCache>().GetCache()
        } {}

    std::string HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;



private:

    mutable userver::cache::LruCacheWrapper<
        cache::calendar::UserId,
        cache::calendar::CalendarData
    > _calendar_cache;

};

} // namespace priemman::handlers::workspace
