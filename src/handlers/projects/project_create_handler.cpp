#include "project_create_handler.hpp"

#include <proto/project.pb.h>
#include <userver/server/http/http_method.hpp>

#include <src/handlers/projects/project_helpers.hpp>
#include <src/handlers/projects/project_proto_convert.hpp>

namespace priemman::handlers::projects {

namespace {
using namespace userver::server::http;  // NOLINT
}  // namespace

std::string ProjectCreateHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&
) const {
    auto& res = request.GetHttpResponse();
    res.SetContentType("application/x-protobuf");

    auto user_id = RequireAuth(request);
    if (!user_id.has_value()) {
        return ErrorResult("UNAUTHORIZED", "Missing or invalid session token");
    }

    if (request.GetMethod() != HttpMethod::kPost) {
        res.SetStatus(HttpStatus::kMethodNotAllowed);
        return ErrorResult("METHOD_NOT_ALLOWED", "Unsupported method");
    }

    priemman::v1::CreateProjectRequest req;
    if (!req.ParseFromString(request.RequestBody()) || !req.has_input()) {
        res.SetStatus(HttpStatus::kBadRequest);
        return ErrorResult("INVALID_BODY", "Invalid request body");
    }

    const auto& input = req.input();
    if (input.title().empty()) {
        res.SetStatus(HttpStatus::kBadRequest);
        return ErrorResult("INVALID_TITLE", "Title is required");
    }

    // Validasi media Cloudinary: harus tercatat, milik sendiri, dan masih orphan.
    // Ditolak tegas sebelum project dibuat supaya tidak ada project setengah jadi.
    const auto public_ids = helpers::CollectPublicIds(input.media());
    if (const auto err = helpers::ValidateMediaForCreate(_media, public_ids, *user_id);
        !err.empty()) {
        res.SetStatus(HttpStatus::kBadRequest);
        return ErrorResult("INVALID_MEDIA", err);
    }

    // Slug unik per owner
    const std::string base_slug = helpers::Slugify(input.title());
    std::string slug = base_slug;
    for (int i = 2; _projects.ExistsByOwnerSlug(*user_id, slug); ++i) {
        slug = base_slug + "-" + std::to_string(i);
    }

    database::ProjectRow row;
    row.owner_id = *user_id;
    row.title = input.title();
    row.slug = slug;
    row.description = input.description();
    row.visibility = mapper::VisibilityToString(input.visibility());
    row.status = mapper::StatusToString(input.status());

    const std::string id = _projects.Create(row);
    helpers::SaveChildren(_projects, id, input);
    helpers::AttachMedia(_media, public_ids, *user_id);

    auto created = _projects.FindById(id);
    priemman::v1::ProjectResponse response;
    if (created.has_value()) {
        *response.mutable_project() = mapper::ToProto(
            *created,
            _projects.ListStrings(id, "tools"),
            _projects.ListStrings(id, "disciplines"),
            _projects.ListStrings(id, "tags"),
            _projects.ListMedia(id),
            _projects.ListCollaborators(id)
        );
    }
    return response.SerializeAsString();
}

}  // namespace priemman::handlers::projects
