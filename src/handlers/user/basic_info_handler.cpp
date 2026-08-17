#include "basic_info_handler.hpp"

#include <proto/user.pb.h>
#include <userver/server/http/http_method.hpp>

#include <src/handlers/user/proto_convert.hpp>

namespace priemman::handlers::user {

namespace {

using namespace userver::server::http;  // NOLINT

std::string BuildMe(
    const AuthenticatedHandlerBase* /*tag*/,
    const database::UserRepository& users,
    const database::AccountRepository& accounts,
    const std::string& user_id
) {
    auto user = users.FindById(user_id);
    if (!user.has_value()) return "";

    const auto about = users.FindAbout(user_id);
    const auto wx = accounts.ListWorkExperiences(user_id);
    const auto ca = accounts.ListConnectedAccounts(user_id);

    return mapper::ToUserProto(*user, about, wx, ca).SerializeAsString();
}

}  // namespace

std::string BasicInfoHandler::HandleRequestThrow(
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

    if (method == HttpMethod::kGet) {
        const auto body = BuildMe(nullptr, _users, _accounts, *user_id);
        if (body.empty()) {
            res.SetStatus(HttpStatus::kNotFound);
            return ErrorResult("NOT_FOUND", "User not found");
        }
        return body;
    }

    if (method == HttpMethod::kPut || method == HttpMethod::kPatch) {
        priemman::v1::UpdateBasicInfoRequest req;
        if (!req.ParseFromString(request.RequestBody())) {
            res.SetStatus(HttpStatus::kBadRequest);
            return ErrorResult("INVALID_BODY", "Invalid request body");
        }

        database::BasicInfoPatch patch;
        patch.first_name = req.first_name();
        patch.last_name = req.last_name();
        patch.headline = req.headline();
        patch.company = req.company();
        patch.country = req.location().country();
        patch.city = req.location().city();
        patch.website_url = req.website_url();

        _users.UpdateBasicInfo(*user_id, patch);

        const auto body = BuildMe(nullptr, _users, _accounts, *user_id);
        return body;
    }

    res.SetStatus(HttpStatus::kMethodNotAllowed);
    return ErrorResult("METHOD_NOT_ALLOWED", "Unsupported method");
}

}  // namespace priemman::handlers::user
