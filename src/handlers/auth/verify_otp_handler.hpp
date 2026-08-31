#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/mysql/cluster.hpp>
#include <userver/storages/mysql/component.hpp>

#include <src/database/otp_repository.hpp>
#include <src/database/session_repository.hpp>
#include <src/database/user_repository.hpp>
#include <userver/yaml_config/yaml_config.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace priemman::auth {

class VerifyOtpHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-verify-otp";

    VerifyOtpHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );

    std::string HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;

    static userver::yaml_config::Schema GetStaticConfigSchema();

private:
    std::string _domain{""};
    std::shared_ptr<userver::storages::mysql::Cluster> _mysql_cluster;
    database::OtpRepository _otp_repo;
    database::UserRepository _users;
    database::SessionRepository _sessions;
};

}  // namespace priemman::auth
