#pragma once
#include <memory>
#include <string>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/storages/mysql/cluster.hpp>
#include <userver/storages/mysql/component.hpp>

#include <src/component/OAuth/Github/OAuthGithubComponent.hpp>
#include <src/component/OAuth/Google/OAuthGoogleComponent.hpp>
#include <src/database/session_repository.hpp>
#include <src/database/user_repository.hpp>

namespace priemman::auth {

class OAuthGoogleCallbackHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-callback-google-oauth";
    OAuthGoogleCallbackHandler(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context);
    std::string HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext& context) const override;
private:
    std::shared_ptr<userver::storages::mysql::Cluster> _mysql_cluster{nullptr};
    database::UserRepository _users;
    database::SessionRepository _sessions;
    daberdev::components::OAuthGoogleComponent* _oauth_google_component{nullptr};
    std::string _redirect_url;
};

class OAuthGithubCallbackHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-callback-github-oauth";
    OAuthGithubCallbackHandler(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context);
    std::string HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext& context) const override;
private:
    std::shared_ptr<userver::storages::mysql::Cluster> _mysql_cluster{nullptr};
    database::UserRepository _users;
    database::SessionRepository _sessions;
    daberdev::components::OAuthGithubComponent* _oauth_github_component{nullptr};
    std::string _redirect_url;
};

}
