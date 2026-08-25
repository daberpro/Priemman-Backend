#include "oauth_callback_handler.hpp"
#include "session_cookie.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <ranges>
#include <src/handlers/api_errors.hpp>
#include <userver/formats/json.hpp>
#include <userver/http/content_type.hpp>
#include <userver/server/http/http_response.hpp>
#include <userver/server/http/http_response_cookie.hpp>

namespace priemman::auth {

namespace {
using userver::formats::json::Value;

std::string JsonString(const Value& doc, const char* key) {
    if (!doc.HasMember(key)){
        return "";
    }
    const auto node = doc[key];
    if (node.IsNull()){
        return "";
    }
    return node.As<std::string>();
}

bool JsonBool(const Value& doc, const char* key, bool default_value = false) {
    if (!doc.HasMember(key)){
        return default_value;
    }
    const auto node = doc[key];
    if (node.IsNull()){
        return default_value;
    }
    return node.As<bool>();
}

std::string NormalizeEmail(std::string email) {
    std::ranges::transform(email, email.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return email;
}

std::string ErrorResult(userver::server::http::HttpResponse& res,
                        userver::server::http::HttpStatus status,
                        std::string_view code) {
    res.SetStatus(status);
    res.SetContentType(errors::kProtobufContentType);
    return errors::BuildErrorResult(code);
}

}

DashboardUrls DashboardUrls::FromConfig(const userver::components::ComponentConfig& config) {
    return DashboardUrls{
        config["dashboard-url"].As<std::string>("http://localhost:3000/dashboard"),
        config["dashboard-creator-url"].As<std::string>("http://localhost:3000/dashboard-creator"),
        config["dashboard-admin-url"].As<std::string>("http://localhost:3000/dashboard-admin"),
    };
}

const std::string& DashboardUrls::ForRole(const std::string& role) const {
    if (role == "admin") return admin;
    if (role == "creator") return creator;
    return user;
}

namespace {

void AddDashboardUrlProperties(userver::yaml_config::Schema& schema) {
    if (!schema.properties.has_value()) {
        schema.properties.emplace();
    }
    static const std::pair<std::string_view, std::string_view> kProps[] = {
        {"dashboard-url", "Dashboard URL for regular users after successful login"},
        {"dashboard-creator-url", "Dashboard URL for creators after successful login"},
        {"dashboard-admin-url", "Dashboard URL for admins after successful login"},
    };
    for (const auto& [name, description] : kProps) {
        schema.properties->emplace(
            std::string{name},
            userver::yaml_config::SchemaPtr(
                userver::yaml_config::impl::SchemaFromString(
                    "type: string\ndescription: " + std::string{description} + "\n")
            )
        );
    }
}

}  // namespace

userver::yaml_config::Schema OAuthGoogleCallbackHandler::GetStaticConfigSchema() {
    auto schema = userver::server::handlers::HttpHandlerBase::GetStaticConfigSchema();
    AddDashboardUrlProperties(schema);
    return schema;
}

userver::yaml_config::Schema OAuthGithubCallbackHandler::GetStaticConfigSchema() {
    auto schema = userver::server::handlers::HttpHandlerBase::GetStaticConfigSchema();
    AddDashboardUrlProperties(schema);
    return schema;
}

// Google
OAuthGoogleCallbackHandler::OAuthGoogleCallbackHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : userver::server::handlers::HttpHandlerBase(config, context),
      _mysql_cluster(context.FindComponent<userver::storages::mysql::Component>("database").GetCluster()),
      _users(&_mysql_cluster),
      _sessions(&_mysql_cluster),
      _oauth_google_component(&context.FindComponent<daberdev::components::OAuthGoogleComponent>("daberdev-oauth-google-component")),
      _dashboards(DashboardUrls::FromConfig(config)) {}

std::string OAuthGoogleCallbackHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&) const {

    auto& res = request.GetHttpResponse();

    // 1. Validasi State dari Cookie (Bukan dari DB)
    const auto url_state = std::string{request.GetArg("state")};
    const std::string cookie_state = request.GetCookie("oauth_state");

    if (url_state.empty() || cookie_state.empty() || url_state != cookie_state) {
        return ErrorResult(res, userver::server::http::HttpStatus::kBadRequest, "INVALID_OR_EXPIRED_STATE");
    }

    // 2. Ambil Data User
    const auto doc = userver::formats::json::FromString(_oauth_google_component->GetData(request));

    database::OAuthUserData oauth;
    oauth.provider = "GOOGLE";
    oauth.provider_user_id = JsonString(doc, "sub");
    oauth.email = NormalizeEmail(JsonString(doc, "email"));
    oauth.name = JsonString(doc, "name");
    oauth.avatar_url = JsonString(doc, "picture");
    oauth.email_verified = JsonBool(doc, "email_verified");

    if (oauth.provider_user_id.empty() || oauth.email.empty()) {
        return ErrorResult(res, userver::server::http::HttpStatus::kUnauthorized, "OAUTH_PROFILE_INCOMPLETE");
    }

    // 3. Find / Create User
    auto result = _users.FindOrCreateFromOAuth(oauth);

    // 4. Buat Session
    auto session = _sessions.Create(result.user.id);

    // 5. Set Cookie & Redirect
    res.SetHeader(std::string("Set-Cookie"), BuildSessionCookie(session.token));

    // Hapus cookie oauth_state karena sudah tidak dipakai
    userver::server::http::Cookie clear_cookie{"oauth_state", ""};
    clear_cookie.SetMaxAge(std::chrono::seconds(0));
    clear_cookie.SetPath("/");
    res.SetCookie(clear_cookie);

    res.SetStatus(userver::server::http::HttpStatus::kFound);
    res.SetHeader(std::string("Location"), _dashboards.ForRole(result.user.role));

    return "";
}

// Github
OAuthGithubCallbackHandler::OAuthGithubCallbackHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : userver::server::handlers::HttpHandlerBase(config, context),
      _mysql_cluster(context.FindComponent<userver::storages::mysql::Component>("database").GetCluster()),
      _users(&_mysql_cluster),
      _sessions(&_mysql_cluster),
      _oauth_github_component(&context.FindComponent<daberdev::components::OAuthGithubComponent>("daberdev-oauth-github-component")),
      _dashboards(DashboardUrls::FromConfig(config)) {}

std::string OAuthGithubCallbackHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&) const {

    auto& res = request.GetHttpResponse();

    const auto url_state = std::string{request.GetArg("state")};
    const std::string cookie_state = request.GetCookie("oauth_state");

    if (url_state.empty() || cookie_state.empty() || url_state != cookie_state) {
        return ErrorResult(res, userver::server::http::HttpStatus::kBadRequest, "INVALID_OR_EXPIRED_STATE");
    }

    const auto doc = userver::formats::json::FromString(_oauth_github_component->GetData(request));

    database::OAuthUserData oauth;
    oauth.provider = "GITHUB";
    if (doc.HasMember("id") && !doc["id"].IsNull()) {
        const auto& id_node = doc["id"];
        oauth.provider_user_id = id_node.IsString() ? id_node.As<std::string>() : std::to_string(id_node.As<std::int64_t>());
    }
    const auto login = JsonString(doc, "login");
    oauth.email = NormalizeEmail(JsonString(doc, "email"));
    oauth.email_verified = JsonBool(doc, "email_verified");
    if (oauth.email.empty() && !oauth.provider_user_id.empty()) {
        oauth.email = oauth.provider_user_id + "+" + login + "@users.noreply.github.com";
        oauth.email_verified = true;
    }
    oauth.name = JsonString(doc, "name");
    if (oauth.name.empty()) oauth.name = login;
    oauth.avatar_url = JsonString(doc, "avatar_url");

    if (oauth.provider_user_id.empty() || oauth.email.empty()) {
        return ErrorResult(res, userver::server::http::HttpStatus::kUnauthorized, "OAUTH_PROFILE_INCOMPLETE");
    }

    auto result = _users.FindOrCreateFromOAuth(oauth);
    auto session = _sessions.Create(result.user.id);

    res.SetHeader(std::string("Set-Cookie"), BuildSessionCookie(session.token));

    userver::server::http::Cookie clear_cookie{"oauth_state", ""};
    clear_cookie.SetMaxAge(std::chrono::seconds(0));
    clear_cookie.SetPath("/");
    res.SetCookie(clear_cookie);

    res.SetStatus(userver::server::http::HttpStatus::kFound);
    res.SetHeader(std::string("Location"), _dashboards.ForRole(result.user.role));

    return "";
}

}
