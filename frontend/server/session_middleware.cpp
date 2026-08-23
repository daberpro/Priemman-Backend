#include "session_middleware.hpp"

#include <optional>

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/mysql/component.hpp>
#include <userver/yaml_config/yaml_config.hpp>

namespace priemman::frontend {

SessionMiddleware::SessionMiddleware(
    std::shared_ptr<userver::storages::mysql::Cluster> cluster
)
    : _cluster(std::move(cluster)),
      _sessions(&_cluster) {
}

void SessionMiddleware::HandleRequest(
    userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext& context
) const {
    std::optional<database::SessionIdentity> identity;

    const std::string path = request.GetRequestPath();
    if (!path.starts_with("/_next/") && request.HasCookie("session")) {
        const std::string& token = request.GetCookie("session");
        if (!token.empty()) {
            try {
                identity = _sessions.FindIdentityByToken(token);
            } catch (const std::exception& e) {
                LOG_WARNING() << "Session lookup failed: " << e;
            }
        }
    }

    context.SetData<std::optional<database::SessionIdentity>>(
        std::string{kSessionDataKey}, std::move(identity)
    );

    Next(request, context);
}

SessionMiddlewareFactory::SessionMiddlewareFactory(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : HttpMiddlewareFactoryBase(config, context),
      _cluster(
          context.FindComponent<userver::storages::mysql::Component>("database").GetCluster()
      ) {
}

userver::yaml_config::Schema SessionMiddlewareFactory::GetStaticConfigSchema() {
    return userver::yaml_config::impl::SchemaFromString(R"(
type: object
description: Session middleware config
additionalProperties: false
properties: {}
)");
}

std::unique_ptr<userver::server::middlewares::HttpMiddlewareBase> SessionMiddlewareFactory::Create(
    const userver::server::handlers::HttpHandlerBase&,
    userver::yaml_config::YamlConfig
) const {
    return std::make_unique<SessionMiddleware>(_cluster);
}

}  // namespace priemman::frontend
