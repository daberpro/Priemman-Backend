#include "collection_handler.hpp"

#include <proto/project.pb.h>
#include <userver/server/http/http_method.hpp>

#include <src/handlers/projects/project_proto_convert.hpp>

namespace priemman::handlers::projects {

namespace {
using namespace userver::server::http;  // NOLINT
}  // namespace

std::string CollectionHandler::HandleRequestThrow(
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

    // ================= LIST =================
    if (method == HttpMethod::kGet) {
        // ?id=...  -> detail; tanpa id -> list semua milik user
        const std::string detail_id{request.GetArg("id")};

        if (!detail_id.empty()) {
            auto row = _collections.FindById(detail_id);
            if (!row.has_value() || row->owner_id != *user_id) {
                res.SetStatus(HttpStatus::kNotFound);
                return ErrorResult("NOT_FOUND", "Collection not found");
            }
            priemman::v1::CollectionResponse response;
            *response.mutable_collection() = mapper::ToProto(
                *row, _collections.ListProjectIds(detail_id));
            return response.SerializeAsString();
        }

        const auto rows = _collections.ListByOwner(*user_id);
        priemman::v1::ListCollectionsResponse response;
        for (const auto& row : rows) {
            *response.add_collections() = mapper::ToProto(
                row, _collections.ListProjectIds(row.id));
        }
        return response.SerializeAsString();
    }

    // ================= CREATE =================
    if (method == HttpMethod::kPost) {
        priemman::v1::CreateCollectionRequest req;
        if (!req.ParseFromString(request.RequestBody()) || !req.has_input()) {
            res.SetStatus(HttpStatus::kBadRequest);
            return ErrorResult("INVALID_BODY", "Invalid request body");
        }

        const auto& input = req.input();
        if (input.title().empty()) {
            res.SetStatus(HttpStatus::kBadRequest);
            return ErrorResult("INVALID_TITLE", "Title is required");
        }

        database::CollectionRow row;
        row.owner_id = *user_id;
        row.title = input.title();
        row.description = input.description();
        row.visibility = input.visibility() ==
                priemman::v1::COLLECTION_VISIBILITY_PUBLIC
            ? "PUBLIC" : "PRIVATE";

        const std::string id = _collections.Create(row);

        std::vector<std::string> project_ids;
        for (const auto& pid : input.project_ids()) {
            project_ids.push_back(pid.value());
        }
        _collections.ReplaceProjects(id, project_ids);

        // Ganti blok designated-initializer dengan fetch ulang:
        auto created = _collections.FindById(id);
        priemman::v1::CollectionResponse response;
        if (created.has_value()) {
            *response.mutable_collection() = mapper::ToProto(*created, project_ids);
        }
        return response.SerializeAsString();
    }

    // ================= UPDATE =================
    if (method == HttpMethod::kPut) {
        priemman::v1::UpdateCollectionRequest req;
        if (!req.ParseFromString(request.RequestBody()) || !req.has_input()) {
            res.SetStatus(HttpStatus::kBadRequest);
            return ErrorResult("INVALID_BODY", "Invalid request body");
        }

        const std::string id = req.id().value();
        const auto& input = req.input();

        database::CollectionRow row;
        row.id = id;
        row.owner_id = *user_id;
        row.title = input.title();
        row.description = input.description();
        row.visibility = input.visibility() ==
                priemman::v1::COLLECTION_VISIBILITY_PUBLIC
            ? "PUBLIC" : "PRIVATE";

        if (!_collections.Update(row)) {
            res.SetStatus(HttpStatus::kNotFound);
            return ErrorResult("NOT_FOUND", "Collection not found");
        }

        std::vector<std::string> project_ids;
        for (const auto& pid : input.project_ids()) {
            project_ids.push_back(pid.value());
        }
        _collections.ReplaceProjects(id, project_ids);

        auto updated = _collections.FindById(id);
        priemman::v1::CollectionResponse response;
        if (updated.has_value()) {
            *response.mutable_collection() = mapper::ToProto(
                *updated, _collections.ListProjectIds(id));
        }
        return response.SerializeAsString();
    }

    // ================= DELETE =================
    if (method == HttpMethod::kDelete) {
        priemman::v1::DeleteCollectionRequest req;
        if (!req.ParseFromString(request.RequestBody()) ||
            req.id().value().empty()) {
            res.SetStatus(HttpStatus::kBadRequest);
            return ErrorResult("INVALID_BODY", "Invalid request body");
        }

        if (!_collections.Delete(req.id().value(), *user_id)) {
            res.SetStatus(HttpStatus::kNotFound);
            return ErrorResult("NOT_FOUND", "Collection not found");
        }

        priemman::v1::DeleteResponse response;
        response.set_success(true);
        return response.SerializeAsString();
    }

    res.SetStatus(HttpStatus::kMethodNotAllowed);
    return ErrorResult("METHOD_NOT_ALLOWED", "Unsupported method");
}

}  // namespace priemman::handlers::projects
