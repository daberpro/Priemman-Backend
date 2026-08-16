#pragma once
#include <userver/formats/json/value.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/components/component_context.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/formats/json.hpp>

#include <src/component/OAuth/Github/OAuthGithubComponent.hpp>
#include <src/component/OAuth/Google/OAuthGoogleComponent.hpp>

namespace priemman::auth {

    class InitiateGoogleOAuthHandler final : public userver::server::handlers::HttpHandlerBase {
    public:
        static constexpr std::string_view kName = "handler-initiate-google-oauth";
        InitiateGoogleOAuthHandler(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context)
        : userver::server::handlers::HttpHandlerBase(config, context),
          _oauth_google_component(&context.FindComponent<daberdev::components::OAuthGoogleComponent>("daberdev-oauth-google-component")) {};

        std::string HandleRequestThrow(
            const userver::server::http::HttpRequest& request,
            userver::server::request::RequestContext& context) const override;

    private:
        daberdev::components::OAuthGoogleComponent* _oauth_google_component{nullptr};

    };

    class InitiateGithubOAuthHandler final : public userver::server::handlers::HttpHandlerBase {
    public:
        static constexpr std::string_view kName = "handler-initiate-github-oauth";
        InitiateGithubOAuthHandler(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context)
        : userver::server::handlers::HttpHandlerBase(config, context),
          _oauth_github_component(&context.FindComponent<daberdev::components::OAuthGithubComponent>("daberdev-oauth-github-component")) {};

        std::string HandleRequestThrow(
            const userver::server::http::HttpRequest& request,
            userver::server::request::RequestContext& context) const override;

    private:
        daberdev::components::OAuthGithubComponent* _oauth_github_component{nullptr};
    };


}
