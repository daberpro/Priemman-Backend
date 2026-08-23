#pragma once

#include <memory>
#include <string_view>

#include <userver/server/middlewares/http_middleware_base.hpp>
#include <userver/storages/mysql/cluster.hpp>

#include <src/database/session_repository.hpp>

namespace priemman::frontend {

inline constexpr std::string_view kSessionDataKey = "session_identity";

class SessionMiddleware final : public userver::server::middlewares::HttpMiddlewareBase {
public:
    static constexpr std::string_view kName = "session-middleware";

    explicit SessionMiddleware(std::shared_ptr<userver::storages::mysql::Cluster> cluster);

protected:
    void HandleRequest(
        userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;

private:
    std::shared_ptr<userver::storages::mysql::Cluster> _cluster;
    database::SessionRepository _sessions;
};

class SessionMiddlewareFactory final : public userver::server::middlewares::HttpMiddlewareFactoryBase {
public:
    static constexpr std::string_view kName = SessionMiddleware::kName;

    SessionMiddlewareFactory(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );

    static userver::yaml_config::Schema GetStaticConfigSchema();

private:
    std::unique_ptr<userver::server::middlewares::HttpMiddlewareBase> Create(
        const userver::server::handlers::HttpHandlerBase& handler,
        userver::yaml_config::YamlConfig middleware_config
    ) const override;

    std::shared_ptr<userver::storages::mysql::Cluster> _cluster;
};

}  // namespace priemman::frontend
