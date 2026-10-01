#pragma once

#include <src/handlers/user/authenticated_handler_base.hpp>
#include <src/database/cache/project_cache_lru.hpp>

#include <userver/cache/lru_cache_component_base.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>

namespace priemman::handlers::projects {

class ProjectDetailHandler final : public AuthenticatedHandlerBase {
public:
    static constexpr std::string_view kName = "handler-project-detail";

    ProjectDetailHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    ) : AuthenticatedHandlerBase(config, context),
        _project_cache{
            context.FindComponent<cache::project::ProjectCache>().GetCache()
        } {}

    std::string HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;

private:
    mutable userver::cache::LruCacheWrapper<
        cache::project::ProjectId,
        cache::project::ProjectData
    > _project_cache;
};

}  // namespace priemman::handlers::projects