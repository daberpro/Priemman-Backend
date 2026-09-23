#include "public_projects_list_handler.hpp"

namespace {
    void FillProject(
        const priemman::database::ProjectRepository& repo,
        const priemman::database::ProjectRowPopulated& row,
        priemman::v1::Project* out
    ) {
        *out = priemman::handlers::mapper::ToProto(
            row,
            repo.ListStrings(row.project.id, "tags"),
            repo.ListMedia(row.project.id),
            repo.ListCollaborators(row.project.id)
        );
    }
}

namespace priemman::handlers::user {

    PublicProjectsListHandler::PublicProjectsListHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    ): AuthenticatedHandlerBase(config,context) {}

    std::string PublicProjectsListHandler::HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext&
    ) const {

        auto& res = request.GetHttpResponse();
        res.SetContentType("application/x-protobuf");

        if (request.GetMethod() != userver::server::http::HttpMethod::kGet) {
            res.SetStatus(userver::server::http::HttpStatus::kMethodNotAllowed);
            return ErrorResult("METHOD_NOT_ALLOWED", "Unsupported method");
        }

        const auto limit = priemman::utils::ParsePageSize(request);
        const auto offset = priemman::utils::ParseOffset(request);
        const auto user_id{request.GetArg("user_id")};

        std::vector<database::ProjectRowPopulated> rows;
        rows = _projects.ListByOwner(
            user_id,
            "PUBLISHED",
            limit, offset
        );
        priemman::v1::ListProjectsResponse response;
        for (const auto& row : rows) {
            FillProject(_projects, row, response.add_projects());
        }

        if (static_cast<std::int64_t>(rows.size()) == limit) {
            response.set_next_page_token(std::to_string(offset + limit));
        }

        return response.SerializeAsString();

    };

}