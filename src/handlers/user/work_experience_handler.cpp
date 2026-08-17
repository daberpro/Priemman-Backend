#include "work_experience_handler.hpp"

#include <proto/user.pb.h>
#include <userver/server/http/http_method.hpp>

#include <src/handlers/user/proto_convert.hpp>

namespace priemman::handlers::user {

namespace {
using namespace userver::server::http;  // NOLINT
}  // namespace

std::string WorkExperienceHandler::HandleRequestThrow(
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
        const auto rows = _accounts.ListWorkExperiences(*user_id);

        priemman::v1::ListWorkExperienceResponse response;
        for (const auto& row : rows) {
            *response.add_entries() = mapper::ToProto(row);
        }
        return response.SerializeAsString();
    }

    // ---------- UPSERT (create / update) ----------
    if (method == HttpMethod::kPost) {
        priemman::v1::UpsertWorkExperienceRequest req;
        if (!req.ParseFromString(request.RequestBody()) || !req.has_entry()) {
            res.SetStatus(HttpStatus::kBadRequest);
            return ErrorResult("INVALID_BODY", "Invalid request body");
        }

        auto row = mapper::ToRow(req.entry());
        const std::string id = _accounts.UpsertWorkExperience(*user_id, row);

        row.id = id;
        return mapper::ToProto(row).SerializeAsString();
    }

    // ---------- DELETE ----------
    if (method == HttpMethod::kDelete) {
        priemman::v1::DeleteWorkExperienceRequest req;
        if (!req.ParseFromString(request.RequestBody()) || req.id().value().empty()) {
            res.SetStatus(HttpStatus::kBadRequest);
            return ErrorResult("INVALID_BODY", "Invalid request body");
        }

        const bool deleted = _accounts.DeleteWorkExperience(*user_id, req.id().value());
        if (!deleted) {
            res.SetStatus(HttpStatus::kNotFound);
            return ErrorResult("NOT_FOUND", "Work experience not found");
        }

        priemman::v1::DeleteResponse response;
        response.set_success(true);
        return response.SerializeAsString();
    }

    res.SetStatus(HttpStatus::kMethodNotAllowed);
    return ErrorResult("METHOD_NOT_ALLOWED", "Unsupported method");
}

}  // namespace priemman::handlers::user
