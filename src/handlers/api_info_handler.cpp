#include "api_info_handler.hpp"

#include <string_view>
#include <userver/formats/json/value_builder.hpp>
#include <userver/formats/json/serialize.hpp>

namespace priemman {

namespace {

constexpr std::string_view kApiName = "Priemman API";
constexpr std::string_view kApiVersion = "1.0.0";
constexpr std::string_view kApiMadeBy = "daberpro";
constexpr std::string_view kApiDescription =
    "REST API backend for the Priemman platform, providing authentication (OTP & OAuth), "
    "user profiles, projects, collections, and media management.";
constexpr std::string_view kApiFramework = "C++23 + userver";
constexpr std::string_view kApiRepository = "https://github.com/daberpro/Priemman-Backend";

struct EndpointInfo {
    std::string_view method;
    std::string_view path;
    std::string_view description;
};

constexpr EndpointInfo kEndpoints[] = {
    {"GET", "/ping", "Health check"},
    {"GET", "/v1/info", "API information (this endpoint)"},
    {"POST", "/v1/auth/send-otp", "Send OTP verification code to email"},
    {"POST", "/v1/auth/verify-otp", "Verify an OTP code and start a session"},
    {"POST", "/v1/auth/logout", "Logout the current session"},
    {"GET", "/v1/auth/google", "Initiate Google OAuth flow"},
    {"GET", "/v1/auth/callback/google", "Google OAuth callback"},
    {"GET", "/v1/auth/github", "Initiate GitHub OAuth flow"},
    {"GET", "/v1/auth/callback/github", "GitHub OAuth callback"},
    {"GET", "/v1/users/me", "Get/update basic user info"},
    {"GET", "/v1/users/me/work-experiences", "Manage work experiences"},
    {"GET", "/v1/users/me/connected-accounts", "Manage connected accounts"},
    {"POST", "/v1/projects", "Create a project"},
    {"GET", "/v1/projects/list", "List projects"},
    {"GET", "/v1/projects/{id}", "Get/update/delete a project"},
    {"GET", "/v1/collections", "Manage collections"},
    {"POST", "/v1/media/upload", "Upload media"},
};

}  // namespace

std::string ApiInfoHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&) const {
    auto& http_response = request.GetHttpResponse();
    http_response.SetContentType("application/json");

    userver::formats::json::ValueBuilder builder;
    builder["name"] = std::string{kApiName};
    builder["version"] = std::string{kApiVersion};
    builder["made_by"] = std::string{kApiMadeBy};
    builder["description"] = std::string{kApiDescription};
    builder["framework"] = std::string{kApiFramework};
    builder["repository"] = std::string{kApiRepository};
    builder["status_url"] = "/ping";

    userver::formats::json::ValueBuilder endpoints;
    for (const auto& endpoint : kEndpoints) {
        userver::formats::json::ValueBuilder item;
        item["method"] = std::string{endpoint.method};
        item["path"] = std::string{endpoint.path};
        item["description"] = std::string{endpoint.description};
        endpoints.PushBack(item.ExtractValue());
    }
    builder["endpoints"] = endpoints;

    return userver::formats::json::ToString(builder.ExtractValue());
}

}  // namespace priemman
