#pragma once

#include <memory>
#include <optional>
#include <string>

#include <proto/common.pb.h>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/http/http_response.hpp>
#include <userver/server/http/http_status.hpp>
#include <userver/storages/mysql/cluster.hpp>
#include <userver/storages/mysql/component.hpp>
#include <userver/clients/http/component.hpp>

#include <src/component/Cloudinary/CloudinaryClient.hpp>
#include <src/database/account_repository.hpp>
#include <src/database/session_repository.hpp>
#include <src/database/user_repository.hpp>
#include <src/database/project_repository.hpp>
#include <src/database/collection_repository.hpp>
#include <src/database/media_repository.hpp>

namespace priemman::handlers {

inline std::string ErrorResult(
    const std::string& code,
    const std::string& message
) {
    priemman::v1::Result r;
    r.set_is_error(true);
    r.set_message(message);
    r.mutable_error_detail()->set_code(code);
    r.mutable_error_detail()->set_message(message);
    return r.SerializeAsString();
}

class AuthenticatedHandlerBase : public userver::server::handlers::HttpHandlerBase {
public:
    AuthenticatedHandlerBase(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    )
        : userver::server::handlers::HttpHandlerBase(config, context),
          _mysql_cluster(
              context.FindComponent<userver::storages::mysql::Component>("database").GetCluster()
          ),
          _sessions(&_mysql_cluster),
          _users(&_mysql_cluster),
          _accounts(&_mysql_cluster),
          _projects(&_mysql_cluster),
          _collections(&_mysql_cluster),
          _media(&_mysql_cluster){
    }

protected:
    std::optional<std::string> RequireAuth(
        const userver::server::http::HttpRequest& request
    ) const {
        auto user_id = TryAuth(request);
        if (!user_id.has_value()) {
            request.GetHttpResponse().SetStatus(
                userver::server::http::HttpStatus::kUnauthorized);
        }
        return user_id;
    }

    // Return user_id kalau token valid, nullopt kalau tidak (tanpa set status).
    std::optional<std::string> TryAuth(
        const userver::server::http::HttpRequest& request
    ) const {
        const auto header = request.GetHeader("Authorization");
        constexpr std::string_view kBearer = "Bearer ";
        if (header.size() <= kBearer.size() ||
            header.compare(0, kBearer.size(), kBearer) != 0) {
            return std::nullopt;
        }
        return _sessions.FindUserIdByToken(
            std::string{header.substr(kBearer.size())}
        );
    }

    std::shared_ptr<userver::storages::mysql::Cluster> _mysql_cluster;
    database::SessionRepository _sessions;
    database::UserRepository _users;
    database::AccountRepository _accounts;
    database::ProjectRepository _projects;
    database::CollectionRepository _collections;
    database::MediaRepository _media;
};

}  // namespace priemman::handlers
