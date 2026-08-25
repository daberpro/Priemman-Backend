#include "verify_otp_handler.hpp"
#include "session_cookie.hpp"
#include "src/database/otp_repository.hpp"

#include <algorithm>
#include <cctype>

#include <proto/auth.pb.h>
#include <string>
#include <src/handlers/api_errors.hpp>
#include <userver/server/http/http_response.hpp>
#include <userver/server/http/http_status.hpp>

namespace priemman::auth {

namespace {
std::string NormalizeEmail(std::string email) {
    std::transform(email.begin(), email.end(), email.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return email;
}
}  // namespace

VerifyOtpHandler::VerifyOtpHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : userver::server::handlers::HttpHandlerBase(config, context),
      _mysql_cluster(
          context.FindComponent<userver::storages::mysql::Component>("database").GetCluster()
      ),
      _otp_repo(&_mysql_cluster),
      _users(&_mysql_cluster),
      _sessions(&_mysql_cluster) {
}

std::string VerifyOtpHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&
) const {
    auto& res = request.GetHttpResponse();

    const auto error_response = [&res](
        userver::server::http::HttpStatus status,
        std::string_view code
    ) {
        res.SetStatus(status);
        res.SetContentType(errors::kProtobufContentType);
        return errors::BuildErrorResult(code);
    };

    priemman::v1::VerifyOtpRequest req;
    if (!req.ParseFromString(request.RequestBody())) {
        return error_response(
            userver::server::http::HttpStatus::kBadRequest,
            "INVALID_REQUEST_BODY");
    }

    const std::string email = NormalizeEmail(req.email());
    const std::string otp_hash = req.otp();
    const std::string ip_address = request.GetRemoteAddress().PrimaryAddressString();

    const database::OtpResult valid = _otp_repo.VerifyAndConsume(email, otp_hash, ip_address);

    switch (valid) {
        case database::OtpResult::kInvalidCode: {
            return error_response(
                userver::server::http::HttpStatus::kUnauthorized,
                "INVALID_OTP_CODE");
        }
        case database::OtpResult::kNotFoundOrExpired: {
            return error_response(
                userver::server::http::HttpStatus::kUnauthorized,
                "INVALID_OR_EXPIRED_OTP");
        }
        case database::OtpResult::kEmailSuspended: {
            return error_response(
                userver::server::http::HttpStatus::kUnauthorized,
                "EMAIL_SUSPENDED");
        }
        case database::OtpResult::kIpSuspended: {
            return error_response(
                userver::server::http::HttpStatus::kUnauthorized,
                "IP_SUSPENDED");
        }
        case database::OtpResult::kSuccess: {
            break;
        }
    }

    auto result = _users.FindOrCreateFromEmail(email);
    auto session = _sessions.Create(result.user.id);

    res.SetHeader(std::string("Set-Cookie"), BuildSessionCookie(session.token));

    priemman::v1::VerifyOtpResponse response;
    response.set_session_token(session.token);
    response.set_is_new_user(result.is_new_user);

    res.SetContentType("application/x-protobuf");
    return response.SerializeAsString();
}

}  // namespace priemman::auth
