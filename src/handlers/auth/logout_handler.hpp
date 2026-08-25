#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <userver/components/component_context.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/storages/mysql/cluster.hpp>
#include <userver/storages/mysql/component.hpp>

#include <src/database/session_repository.hpp>
#include <src/handlers/auth/session_cookie.hpp>

namespace priemman::auth {

class LogoutHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-logout";

    LogoutHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );

    std::string HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;

private:
    std::shared_ptr<userver::storages::mysql::Cluster> _mysql_cluster{nullptr};
    database::SessionRepository _sessions;
};

}
