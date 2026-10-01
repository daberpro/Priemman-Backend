#include "calendar_handler.hpp"

#include <string_view>

#include <userver/http/common_headers.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/http/http_status.hpp>
#include <userver/server/request/request_context.hpp>

namespace priemman::handlers::workspace {

std::string CalendarHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&
) const {
    auto& response = request.GetHttpResponse();
    response.SetContentType(userver::http::content_type::kApplicationJson);

    const auto user_id = TryAuth(request);
    if (!user_id.has_value()) {
        response.SetStatus(userver::server::http::HttpStatus::kUnauthorized);
        return ErrorResult("UNAUTHORIZED", "Unauthorized");
    }

    try {
        auto cached = _calendar_cache.GetOptional(*user_id);
        if (cached.has_value()) return *cached;
        return _calendar_cache.Get(*user_id);
    } catch (const std::runtime_error& error) {
        const std::string_view code{error.what()};
        if (code == "USER_NOT_FOUND") {
            response.SetStatus(userver::server::http::HttpStatus::kNotFound);
            return ErrorResult("NOT_FOUND", "User not found");
        }
        if (code == "PLATFORM_USER_NOT_FOUND") {
            response.SetStatus(userver::server::http::HttpStatus::kNotFound);
            return ErrorResult(
                "NOT_FOUND", "User is not registered on Priemman Platform");
        }
        if (code == "PLATFORM_UNAVAILABLE") {
            response.SetStatus(userver::server::http::HttpStatus::kBadGateway);
            return ErrorResult(
                "BAD_GATEWAY", "Failed to contact Priemman Platform");
        }
        if (code == "CALENDAR_UNAVAILABLE") {
            response.SetStatus(userver::server::http::HttpStatus::kBadGateway);
            return ErrorResult(
                "BAD_GATEWAY",
                "Failed to retrieve calendar from Priemman Platform");
        }
        throw;
    }
}

}  // namespace priemman::handlers::workspace
