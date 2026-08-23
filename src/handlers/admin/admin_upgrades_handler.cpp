#include "admin_upgrades_handler.hpp"

#include <stdexcept>

#include <proto/user.pb.h>
#include <userver/server/http/http_method.hpp>

#include <src/handlers/user/proto_convert.hpp>

namespace priemman::handlers::admin {

namespace {

using namespace userver::server::http;  // NOLINT

}  // namespace

std::string AdminUpgradeListHandler::HandleRequestThrow(
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

    std::string status_filter{request.GetArg("status")};
    if (status_filter.empty()) {
        status_filter = "pending";
    }

    std::vector<database::UpgradeRequestRow> rows;
    try {
        rows = _upgrades.ListByStatus(status_filter);
    } catch (const std::invalid_argument&) {
        res.SetStatus(HttpStatus::kBadRequest);
        return ErrorResult(
            "INVALID_STATUS",
            "Status must be pending, approved, rejected, or paid");
    }

    priemman::v1::AdminListUpgradeRequestsResponse response;
    for (const auto& row : rows) {
        mapper::FillUpgradeRequestEntry(row, response.add_requests());
    }
    return response.SerializeAsString();
}

std::string AdminUpgradeReviewHandler::HandleRequestThrow(
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

    if (request.GetMethod() != HttpMethod::kPost) {
        res.SetStatus(HttpStatus::kMethodNotAllowed);
        return ErrorResult("METHOD_NOT_ALLOWED", "Unsupported method");
    }

    priemman::v1::AdminReviewUpgradeRequest req;
    if (!req.ParseFromString(request.RequestBody()) || req.id().value().empty()) {
        res.SetStatus(HttpStatus::kBadRequest);
        return ErrorResult("INVALID_BODY", "Invalid request body");
    }

    const bool ok = req.approve()
        ? _upgrades.Approve(req.id().value())
        : _upgrades.Reject(req.id().value(), req.rejection_reason());

    if (!ok) {
        res.SetStatus(HttpStatus::kConflict);
        return ErrorResult(
            "REVIEW_FAILED",
            "Request not found or not in pending status");
    }

    priemman::v1::AdminReviewUpgradeResponse response;
    response.set_success(true);
    const auto row = _upgrades.FindByIdWithUser(req.id().value());
    if (row.has_value()) {
        mapper::FillUpgradeRequestEntry(*row, response.mutable_request());
    }
    return response.SerializeAsString();
}

std::string AdminUpgradeConfirmPaymentHandler::HandleRequestThrow(
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

    if (request.GetMethod() != HttpMethod::kPost) {
        res.SetStatus(HttpStatus::kMethodNotAllowed);
        return ErrorResult("METHOD_NOT_ALLOWED", "Unsupported method");
    }

    priemman::v1::AdminConfirmPaymentRequest req;
    if (!req.ParseFromString(request.RequestBody()) || req.id().value().empty()) {
        res.SetStatus(HttpStatus::kBadRequest);
        return ErrorResult("INVALID_BODY", "Invalid request body");
    }

    const auto user_id = _upgrades.ConfirmPaid(req.id().value());
    if (!user_id.has_value()) {
        res.SetStatus(HttpStatus::kConflict);
        return ErrorResult(
            "CONFIRM_FAILED",
            "Request not found or not in approved status");
    }

    _users.SetRole(*user_id, "creator");

    priemman::v1::AdminConfirmPaymentResponse response;
    response.set_success(true);
    response.mutable_user_id()->set_value(*user_id);
    return response.SerializeAsString();
}

}  // namespace priemman::handlers::admin
