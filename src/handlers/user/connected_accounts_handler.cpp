#include "connected_accounts_handler.hpp"

#include <proto/user.pb.h>
#include <userver/server/http/http_method.hpp>

#include <src/handlers/user/proto_convert.hpp>

namespace priemman::handlers::user {

namespace {
using namespace userver::server::http;  // NOLINT

std::string PlatformToString(const priemman::v1::ConnectedPlatform platform) {
    switch (platform) {
        case priemman::v1::CONNECTED_PLATFORM_INSTAGRAM: return "INSTAGRAM";
        case priemman::v1::CONNECTED_PLATFORM_LINKEDIN:  return "LINKEDIN";
        case priemman::v1::CONNECTED_PLATFORM_GITHUB:    return "GITHUB";
        default:                                          return "";
    }
}
}  // namespace

std::string ConnectedAccountsHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&
) const {
    auto& res = request.GetHttpResponse();
    res.SetContentType("application/x-protobuf");

    auto user_id = RequireAuth(request);
    if (!user_id.has_value()) {
        return ErrorResult("UNAUTHORIZED", "Missing or invalid session token");
    }

    const auto method = request.GetMethod();

    // ---------- LIST ----------
    if (method == HttpMethod::kGet) {
        const auto rows = _accounts.ListConnectedAccounts(*user_id);

        priemman::v1::ListConnectedAccountsResponse response;
        for (const auto& row : rows) {
            *response.add_accounts() = mapper::ToProto(row);
        }
        return response.SerializeAsString();
    }

    // ---------- DELETE (disconnect) ----------
    if (method == HttpMethod::kDelete) {
        priemman::v1::DeleteConnectedAccountRequest req;
        if (!req.ParseFromString(request.RequestBody())) {
            res.SetStatus(HttpStatus::kBadRequest);
            return ErrorResult("INVALID_BODY", "Invalid request body");
        }

        const std::string platform = PlatformToString(req.platform());
        if (platform.empty()) {
            res.SetStatus(HttpStatus::kBadRequest);
            return ErrorResult("INVALID_PLATFORM", "Unknown platform");
        }

        const bool deleted = _accounts.DeleteConnectedAccount(*user_id, platform);
        if (!deleted) {
            res.SetStatus(HttpStatus::kNotFound);
            return ErrorResult("NOT_FOUND", "Connected account not found");
        }

        priemman::v1::DeleteResponse response;
        response.set_success(true);
        return response.SerializeAsString();
    }

    res.SetStatus(HttpStatus::kMethodNotAllowed);
    return ErrorResult("METHOD_NOT_ALLOWED", "Unsupported method");
}

}  // namespace priemman::handlers::user
