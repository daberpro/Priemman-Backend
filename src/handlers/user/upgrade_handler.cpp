#include "upgrade_handler.hpp"

#include <proto/user.pb.h>
#include <userver/server/http/http_method.hpp>

#include <src/handlers/user/proto_convert.hpp>

namespace priemman::handlers::user {

namespace {

using namespace userver::server::http;  // NOLINT

}  // namespace

std::string UpgradeHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&
) const {
    auto& res = request.GetHttpResponse();
    res.SetContentType("application/x-protobuf");

    const auto identity = TryAuthIdentity(request);
    if (!identity.has_value()) {
        res.SetStatus(HttpStatus::kUnauthorized);
        return ErrorResult("UNAUTHORIZED", "Missing or invalid session token");
    }

    const auto method = request.GetMethod();

    if (method == HttpMethod::kGet) {
        priemman::v1::UpgradeStatus response;
        const auto row = _upgrades.FindLatestByUser(identity->user_id);
        if (row.has_value()) {
            mapper::FillUpgradeStatus(*row, &response);
        } else {
            response.set_status("none");
        }
        return response.SerializeAsString();
    }

    if (method == HttpMethod::kPost) {
        if (identity->role == "creator" || identity->role == "admin") {
            res.SetStatus(HttpStatus::kConflict);
            return ErrorResult(
                "ALREADY_CREATOR",
                "Account is already a creator or admin");
        }

        if (!_upgrades.CreateRequest(identity->user_id)) {
            res.SetStatus(HttpStatus::kConflict);
            return ErrorResult(
                "UPGRADE_ALREADY_REQUESTED",
                "An upgrade request is already active for this account");
        }

        priemman::v1::CreateUpgradeRequestResponse response;
        response.set_success(true);
        const auto row = _upgrades.FindActiveByUser(identity->user_id);
        if (row.has_value()) {
            mapper::FillUpgradeStatus(*row, response.mutable_status());
        }
        res.SetStatus(HttpStatus::kCreated);
        return response.SerializeAsString();
    }

    res.SetStatus(HttpStatus::kMethodNotAllowed);
    return ErrorResult("METHOD_NOT_ALLOWED", "Unsupported method");
}

}  // namespace priemman::handlers::user
