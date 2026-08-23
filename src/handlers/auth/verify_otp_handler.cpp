#include "verify_otp_handler.hpp"
#include "session_cookie.hpp"
#include "src/database/otp_repository.hpp"

#include <algorithm>
#include <cctype>

#include <proto/auth.pb.h>
#include <string>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
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

    priemman::v1::VerifyOtpRequest req;
    if (!req.ParseFromString(request.RequestBody())) {
        res.SetStatus(userver::server::http::HttpStatus::kBadRequest);
        userver::formats::json::ValueBuilder err;
        err["error"] = "invalid_request_body";
        return userver::formats::json::ToString(err.ExtractValue());
    }

    const std::string email = NormalizeEmail(req.email());
    const std::string otp_hash = req.otp();
    const std::string ip_address = request.GetRemoteAddress().PrimaryAddressString();

    const database::OtpResult valid = _otp_repo.VerifyAndConsume(email, otp_hash, ip_address);

    switch (valid) {
        case database::OtpResult::kInvalidCode:{
            res.SetStatus(userver::server::http::HttpStatus::kUnauthorized);
            userver::formats::json::ValueBuilder err;
            err["error"] = "invalid_otp_code";
            return userver::formats::json::ToString(err.ExtractValue());
        }
        case database::OtpResult::kNotFoundOrExpired:{
            res.SetStatus(userver::server::http::HttpStatus::kUnauthorized);
            userver::formats::json::ValueBuilder err;
            err["error"] = "invalid_or_expired_otp";
            return userver::formats::json::ToString(err.ExtractValue());
        }
        case database::OtpResult::kEmailSuspended:{
            res.SetStatus(userver::server::http::HttpStatus::kUnauthorized);
            userver::formats::json::ValueBuilder err;
            err["error"] = "email_suspended";
            return userver::formats::json::ToString(err.ExtractValue());
        }
        case database::OtpResult::kIpSuspended:{
            res.SetStatus(userver::server::http::HttpStatus::kUnauthorized);
            userver::formats::json::ValueBuilder err;
            err["error"] = "ip_suspended";
            return userver::formats::json::ToString(err.ExtractValue());
        }
        case database::OtpResult::kSuccess:{
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
