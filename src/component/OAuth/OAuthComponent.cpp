#include "OAuthComponent.hpp"
#include <chrono>
#include <userver/clients/http/component.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json.hpp>
#include <userver/http/status_code.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/http/url.hpp>
#include <userver/server/http/http_response_cookie.hpp>
#include <userver/utils/uuid7.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/yaml_config/schema.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace daberdev::components {

OAuthComponent::OAuthComponent(const userver::components::ComponentConfig& config,
                               const userver::components::ComponentContext& context)
    : userver::components::LoggableComponentBase(config, context),
      m_auth_url{config["auth_url"].As<std::string>()},
      m_token_url{config["token_url"].As<std::string>()},
      m_api_url{config["api_url"].As<std::string>()},
      m_redirect_url{config["redirect_url"].As<std::string>()},
      m_client_id{config["client_id"].As<std::string>()},
      m_client_secret{config["client_secret"].As<std::string>()},
      m_scope{config["scope"].As<std::string>()},
      m_client{&context.FindComponent<userver::components::HttpClient>().GetHttpClient()} {

          if(m_redirect_url.empty() || m_auth_url.empty() || m_token_url.empty() ||
             m_api_url.empty() || m_client_id.empty() || m_client_secret.empty()){
              throw std::runtime_error("OAuthComponent: auth_url, token_url, api_url, redirect_url, client_id, and client_secret cannot be empty");
          }
      }

void OAuthComponent::Auth(const userver::server::http::HttpRequest& req) {
    std::string state = userver::utils::generators::GenerateUuidV7();

    userver::server::http::Cookie cookie{"oauth_state", state};
    cookie.SetMaxAge(std::chrono::minutes(5));
    cookie.SetHttpOnly();

    // dev mode only
    // cookie.SetSecure();

    std::string login_url = userver::http::MakeUrl(
        m_auth_url,
        {
            {"client_id", m_client_id},
            {"redirect_uri", m_redirect_url},
            {"response_type", "code"},
            {"scope", m_scope},
            {"state", state}
        }
    );

    auto& response = req.GetHttpResponse();
    response.SetCookie(cookie);
    response.SetStatus(userver::http::StatusCode::kFound);
    response.SetHeader(std::string{"Location"}, login_url);
}

std::string OAuthComponent::GetToken(const userver::server::http::HttpRequest& req) {
    auto code = req.GetArg("code");

    userver::formats::json::ValueBuilder payload;
    payload["client_id"] = m_client_id;
    payload["client_secret"] = m_client_secret;
    payload["code"] = code;
    payload["redirect_uri"] = m_redirect_url;
    payload["grant_type"] = "authorization_code";

    auto token = m_client->CreateRequest()
        .url(m_token_url)
        .data(userver::formats::json::ToString(payload.ExtractValue()))
        .headers({
            {"Accept", "application/json"},
            {"Content-Type", "application/json"}
        })
        .post()
        .timeout(std::chrono::milliseconds(5000))
        .perform();

    token->raise_for_status();
    auto result = userver::formats::json::FromString(token->body());
    if(result.HasMember("error")){
        throw std::runtime_error("OAuth API Error: " + result["error_description"].As<std::string>());
    }

    return result["access_token"].As<std::string>();
}

std::string OAuthComponent::GetData(const userver::server::http::HttpRequest& req) {
    std::string access_token = GetToken(req);
    auto response = m_client->CreateRequest()
        .get(m_api_url)
        .headers({
            {"Authorization", "Bearer " + access_token},
            {"Accept", "application/json"}
        })
        .timeout(std::chrono::milliseconds(5000))
        .perform();

    response->raise_for_status();
    return response->body();
}

userver::yaml_config::Schema OAuthComponent::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<userver::components::LoggableComponentBase>(R"(
    type: object
    description: Universal OAuthComponent config
    additionalProperties: false
    properties:
        auth_url:
            type: string
            description: Authentication URL from provider
        token_url:
            type: string
            description: Token URL from provider
        api_url:
            type: string
            description: API base URL from provider
        redirect_url:
            type: string
            description: Callback redirect URL for this app
        client_id:
            type: string
            description: Application Client ID
        client_secret:
            type: string
            description: Application Client Secret (recommended to use #env)
        scope:
            type: string
            description: Required OAuth scopes (space separated)
    )");
}

}
