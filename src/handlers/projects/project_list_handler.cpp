#include "project_list_handler.hpp"

#include <cstdlib>
#include <string>

#include <proto/project.pb.h>
#include <userver/server/http/http_method.hpp>

#include <src/handlers/projects/project_proto_convert.hpp>

namespace priemman::handlers::projects {

namespace {
using namespace userver::server::http;  // NOLINT

constexpr std::int64_t kDefaultPageSize = 20;
constexpr std::int64_t kMaxPageSize = 50;

std::int64_t ParsePageSize(const userver::server::http::HttpRequest& request) {
    const auto arg = request.GetArg("page_size");
    const auto value = arg.empty() ? 0 : std::atoll(std::string{arg}.c_str());
    if (value <= 0) return kDefaultPageSize;
    return value > kMaxPageSize ? kMaxPageSize : value;
}

std::int64_t ParseOffset(const userver::server::http::HttpRequest& request) {
    const auto token = request.GetArg("page_token");
    const auto value = token.empty() ? 0 : std::atoll(std::string{token}.c_str());
    return value < 0 ? 0 : value;
}

void FillProject(
    const database::ProjectRepository& repo,
    const database::ProjectRow& row,
    priemman::v1::Project* out
) {
    *out = mapper::ToProto(
        row,
        repo.ListStrings(row.id, "tags"),
        repo.ListMedia(row.id),
        repo.ListCollaborators(row.id)
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

    const auto limit = ParsePageSize(request);
    const auto offset = ParseOffset(request);

    // Dengan token  -> list milik sendiri (semua status, bisa difilter)
    // Tanpa token   -> feed publik untuk halaman utama (PUBLISHED + PUBLIC)
    const auto viewer = TryAuth(request);

    std::vector<database::ProjectRow> rows;
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
