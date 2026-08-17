#include "logout_handler.hpp"

#include <proto/auth.pb.h>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_response.hpp>

namespace priemman::auth {

LogoutHandler::LogoutHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : userver::server::handlers::HttpHandlerBase(config, context),
      _mysql_cluster(
          context.FindComponent<userver::storages::mysql::Component>("database").GetCluster()
      ),
      _sessions(&_mysql_cluster) {
}

std::string LogoutHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&
) const {
    auto& res = request.GetHttpResponse();

    priemman::v1::LogoutRequest req;
    if (!req.ParseFromString(request.RequestBody())) {
        res.SetStatus(userver::server::http::HttpStatus::kBadRequest);
        userver::formats::json::ValueBuilder err;
        err["error"] = "invalid_request_body";
        return userver::formats::json::ToString(err.ExtractValue());
    }

    const bool revoked = _sessions.Revoke(req.session_token());
    res.SetHeader(std::string("Set-Cookie"), "session=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0");

    priemman::v1::LogoutResponse response;
    response.set_success(revoked);

    res.SetContentType("application/x-protobuf");
    return response.SerializeAsString();
}

}
