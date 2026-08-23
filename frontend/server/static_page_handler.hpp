#pragma once

#include <userver/components/fs_cache.hpp>
#include <userver/server/handlers/http_handler_base.hpp>

namespace priemman::frontend {

class StaticPageHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    StaticPageHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );

    std::string HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;

    static userver::yaml_config::Schema GetStaticConfigSchema();

private:
    const userver::fs::FsCacheClient* _fs;
};

}  // namespace priemman::frontend
