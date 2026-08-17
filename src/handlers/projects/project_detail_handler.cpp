#include "project_detail_handler.hpp"

#include <proto/project.pb.h>
#include <userver/server/http/http_method.hpp>

#include <src/handlers/projects/project_helpers.hpp>
#include <src/handlers/projects/project_proto_convert.hpp>

namespace priemman::handlers::projects {

namespace {
using namespace userver::server::http;  // NOLINT

bool IsPubliclyVisible(const database::ProjectRow& row) {
    return row.status == "PUBLISHED" && row.visibility == "PUBLIC";
}
}  // namespace

std::string ProjectDetailHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&
) const {
    auto& res = request.GetHttpResponse();
    res.SetContentType("application/x-protobuf");

    const std::string id{request.GetPathArg("id")};
    if (id.empty()) {
        res.SetStatus(HttpStatus::kBadRequest);
        return ErrorResult("INVALID_ID", "Project id is required");
    }

    const auto viewer = TryAuth(request);
    const auto method = request.GetMethod();

    // ================= GET =================
    if (method == HttpMethod::kGet) {
        auto row = _projects.FindById(id);
        if (!row.has_value()) {
            res.SetStatus(HttpStatus::kNotFound);
            return ErrorResult("NOT_FOUND", "Project not found");
        }

        const bool is_owner = viewer.has_value() && *viewer == row->owner_id;
        if (!IsPubliclyVisible(*row) && !is_owner) {
            res.SetStatus(HttpStatus::kNotFound);
            return ErrorResult("NOT_FOUND", "Project not found");
        }

        if (IsPubliclyVisible(*row) && !is_owner) {
            _projects.IncrementViews(id);
        }

        priemman::v1::ProjectResponse response;
        *response.mutable_project() = mapper::ToProto(
            *row,
            _projects.ListStrings(id, "tools"),
            _projects.ListStrings(id, "disciplines"),
            _projects.ListStrings(id, "tags"),
            _projects.ListMedia(id),
            _projects.ListCollaborators(id)
        );
        return response.SerializeAsString();
    }

    // ================= PUT / DELETE (owner only) =================
    auto user_id = RequireAuth(request);
    if (!user_id.has_value()) {
        return ErrorResult("UNAUTHORIZED", "Missing or invalid session token");
    }

    if (method == HttpMethod::kPut) {
        priemman::v1::UpdateProjectRequest req;
        if (!req.ParseFromString(request.RequestBody()) || !req.has_input()) {
            res.SetStatus(HttpStatus::kBadRequest);
            return ErrorResult("INVALID_BODY", "Invalid request body");
        }

        const auto& input = req.input();

        // Cek kepemilikan + ambil slug lama (slug tetap stabil)
        auto existing = _projects.FindById(id);
        if (!existing.has_value() || existing->owner_id != *user_id) {
            res.SetStatus(HttpStatus::kNotFound);
            return ErrorResult("NOT_FOUND", "Project not found");
        }

        database::ProjectRow row;
        row.id = id;
        row.owner_id = *user_id;
        row.title = input.title();
        row.slug = existing->slug;
        row.description = input.description();
        row.visibility = mapper::VisibilityToString(input.visibility());
        row.status = mapper::StatusToString(input.status());

        if (!_projects.Update(row)) {
            res.SetStatus(HttpStatus::kNotFound);
            return ErrorResult("NOT_FOUND", "Project not found");
        }

        helpers::SaveChildren(_projects, id, input);

        auto updated = _projects.FindById(id);
        priemman::v1::ProjectResponse response;
        if (updated.has_value()) {
            *response.mutable_project() = mapper::ToProto(
                *updated,
                _projects.ListStrings(id, "tools"),
                _projects.ListStrings(id, "disciplines"),
                _projects.ListStrings(id, "tags"),
                _projects.ListMedia(id),
                _projects.ListCollaborators(id)
            );
        }
        return response.SerializeAsString();
    }

    if (method == HttpMethod::kDelete) {
        if (!_projects.Delete(id, *user_id)) {
            res.SetStatus(HttpStatus::kNotFound);
            return ErrorResult("NOT_FOUND", "Project not found");
        }
        priemman::v1::DeleteResponse response;
        response.set_success(true);
        return response.SerializeAsString();
    }

    res.SetStatus(HttpStatus::kMethodNotAllowed);
    return ErrorResult("METHOD_NOT_ALLOWED", "Unsupported method");
}

}  // namespace priemman::handlers::projects
