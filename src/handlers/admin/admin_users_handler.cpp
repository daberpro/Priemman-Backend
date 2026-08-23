#include "admin_users_handler.hpp"

#include <charconv>
#include <stdexcept>
#include <vector>

#include <proto/user.pb.h>
#include <userver/server/http/http_method.hpp>

#include <src/handlers/user/proto_convert.hpp>

namespace priemman::handlers::admin {

namespace {

using namespace userver::server::http;  // NOLINT

constexpr std::int64_t kDefaultLimit = 20;
constexpr std::int64_t kMaxLimit = 100;

std::int64_t ParseIntArg(
    const std::string_view value,
    const std::int64_t fallback
) {
    if (value.empty()) {
        return fallback;
    }
    std::int64_t out = fallback;
    const auto [ptr, ec] = std::from_chars(value.begin(), value.end(), out);
    if (ec != std::errc{} || ptr != value.end() || out < 0) {
        return fallback;
    }
    return out;
}

}  // namespace

std::string AdminUsersHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&
) const {
    auto& res = request.GetHttpResponse();
    res.SetContentType("application/x-protobuf");

    if (!RequireAdmin(request).has_value()) {
        const auto status = res.GetStatus();
        return ErrorResult(
            status == HttpStatus::kForbidden ? "FORBIDDEN" : "UNAUTHORIZED",
            status == HttpStatus::kForbidden
                ? "Admin role required"
                : "Missing or invalid session token");
    }

    if (request.GetMethod() != HttpMethod::kGet) {
        res.SetStatus(HttpStatus::kMethodNotAllowed);
        return ErrorResult("METHOD_NOT_ALLOWED", "Unsupported method");
    }

    auto limit = ParseIntArg(request.GetArg("limit"), kDefaultLimit);
    if (limit == 0) {
        limit = kDefaultLimit;
    }
    if (limit > kMaxLimit) {
        limit = kMaxLimit;
    }
    const auto offset = ParseIntArg(request.GetArg("offset"), 0);
    const std::string role_filter{request.GetArg("role")};

    std::vector<database::AdminUserRow> rows;
    std::int64_t total = 0;
    try {
        rows = _users.ListUsers(limit, offset, role_filter);
        total = _users.CountUsers(role_filter);
    } catch (const std::invalid_argument&) {
        res.SetStatus(HttpStatus::kBadRequest);
        return ErrorResult("INVALID_ROLE", "Role filter must be user, creator, or admin");
    }

    priemman::v1::AdminListUsersResponse response;
    for (const auto& row : rows) {
        auto* entry = response.add_users();
        entry->mutable_id()->set_value(row.id);
        entry->set_email(row.email);
        entry->set_first_name(row.first_name);
        entry->set_last_name(row.last_name);
        entry->set_role(mapper::StringToUserRole(row.role));
        mapper::SqlToTimestamp(row.created_at, entry->mutable_created_at());
    }
    response.set_total(static_cast<std::int32_t>(total));
    response.set_limit(static_cast<std::int32_t>(limit));
    response.set_offset(static_cast<std::int32_t>(offset));
    return response.SerializeAsString();
}

}  // namespace priemman::handlers::admin
