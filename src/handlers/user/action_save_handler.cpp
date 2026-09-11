#include "action_save_handler.hpp"
#include <src/handlers/utils.hpp>
#include <src/handlers/user/proto_convert.hpp>
#include <proto/user.pb.h>
#include <proto/common.pb.h>
#include <format>
#include <userver/server/http/http_method.hpp>

namespace {
    using namespace userver::server::http;
}

namespace priemman::handlers::user {

    std::string ActionSaveHandler::HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& /*context*/
    ) const {

        auto& http_response = request.GetHttpResponse();
        http_response.SetContentType("application/x-protobuf");

        auto user_id = RequireAuth(request);
        if (!user_id.has_value()) {
            http_response.SetStatus(HttpStatus::kUnauthorized);
            return ErrorResult("UNAUTHORIZED", "Missing or invalid session token");
        }

        const auto method = request.GetMethod();

        if (method == HttpMethod::kGet) {
            auto limit = utils::ParseInt(request.GetArg("limit")).value_or(10);
            auto offset = utils::ParseInt(request.GetArg("offset")).value_or(0);

            if (limit < 0) {
                http_response.SetStatus(HttpStatus::kBadRequest);
                return ErrorResult("OUT_OF_RANGE", "limit out of range");
            }

            if (offset < 0) {
                http_response.SetStatus(HttpStatus::kBadRequest);
                return ErrorResult("OUT_OF_RANGE", "offset out of range");
            }

            const auto rows = _users.ListSavedProjects(*user_id, limit, offset);
            priemman::v1::ListProjectSummaryRow proto_response;
            for (const auto& row : rows) {
                *proto_response.add_projects() = mapper::ToProto(row);
            }
            return proto_response.SerializeAsString();
        }

        if (method == HttpMethod::kPost || method == HttpMethod::kDelete) {
            priemman::v1::ActionInput req;
            if (!req.ParseFromString(request.RequestBody())) {
                http_response.SetStatus(HttpStatus::kBadRequest);
                return ErrorResult("INVALID_BODY", "Invalid request body");
            }

            auto body_project_id = req.project_id();
            auto project = _projects.FindById(body_project_id);
            if (!project.has_value()) {
                http_response.SetStatus(HttpStatus::kNotFound);
                return ErrorResult("NOT_FOUND", "Project Not Found");
            }

            const auto user = _users.FindById(*user_id);
            if (!user.has_value()) {
                http_response.SetStatus(HttpStatus::kNotFound);
                return ErrorResult("NOT_FOUND", "User not found");
            }

            std::string result_msg = "";
            bool action_executed = false;

            if (method == HttpMethod::kPost) {
                if (_users.ActionSave(user->id, body_project_id)) {
                    result_msg += std::format("Saved project #{}. ", body_project_id);
                    action_executed = true;
                }
            } else {
                if (_users.ActionUnSave(user->id, body_project_id)) {
                    result_msg += std::format("Unsaved project #{}. ", body_project_id);
                    action_executed = true;
                }
            }

            if (action_executed) {
                return utils::BuildResult(result_msg);
            }

            http_response.SetStatus(HttpStatus::kBadRequest);
            return ErrorResult("INVALID_ACTION", "No valid actions performed");
        }

        http_response.SetStatus(HttpStatus::kMethodNotAllowed);
        return ErrorResult("METHOD_NOT_ALLOWED", "HTTP method not supported");
    }

} // namespace priemman::handlers::user
