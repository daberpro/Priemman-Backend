#include "project_list_handler.hpp"
#include "src/database/project_repository.hpp"

#include <cstdlib>
#include <string>

#include <proto/project.pb.h>
#include <userver/server/http/http_method.hpp>

#include <src/handlers/projects/project_proto_convert.hpp>
#include <src/handlers/utils.hpp>

namespace priemman::handlers::projects {

namespace {
using namespace userver::server::http;  // NOLINT

void FillProject(
    const database::ProjectRepository& repo,
    const database::ProjectRowPopulated& row,
    priemman::v1::Project* out
) {
    *out = mapper::ToProto(
        row,
        repo.ListStrings(row.project.id, "tags"),
        repo.ListMedia(row.project.id),
        repo.ListCollaborators(row.project.id)
    );
}

}  // namespace

std::string ProjectListHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&
) const {
    auto& res = request.GetHttpResponse();
    res.SetContentType("application/x-protobuf");

    if (request.GetMethod() != HttpMethod::kGet) {
        res.SetStatus(HttpStatus::kMethodNotAllowed);
        return ErrorResult("METHOD_NOT_ALLOWED", "Unsupported method");
    }

    const auto limit = priemman::utils::ParsePageSize(request);
    const auto offset = priemman::utils::ParseOffset(request);

    // Dengan token  -> list milik sendiri (semua status, bisa difilter)
    // Tanpa token   -> feed publik untuk halaman utama (PUBLISHED + PUBLIC)
    const auto viewer = TryAuth(request);

    std::vector<database::ProjectRowPopulated> rows;
    if (viewer.has_value()) {
        rows = _projects.ListByOwner(
            *viewer,
            std::string{request.GetArg("status")},
            limit, offset
        );
    } else {
        rows = _projects.ListPublic(limit, offset);
    }

    priemman::v1::ListProjectsResponse response;
    for (const auto& row : rows) {
        FillProject(_projects, row, response.add_projects());
    }

    if (static_cast<std::int64_t>(rows.size()) == limit) {
        response.set_next_page_token(std::to_string(offset + limit));
    }

    return response.SerializeAsString();
}

}  // namespace priemman::handlers::projects
