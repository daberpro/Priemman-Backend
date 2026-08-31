#include "logout_handler.hpp"

#include <proto/auth.pb.h>
#include <src/handlers/api_errors.hpp>
#include <userver/server/http/http_response.hpp>
#include <userver/components/component_config.hpp>

namespace priemman::auth {

void AddProperties(userver::yaml_config::Schema& schema) {
    if (!schema.properties.has_value()) {
        schema.properties.emplace();
    }
    static const std::pair<std::string_view, std::string_view> kProps[] = {
        {"domain","Domain utama atau base domain contoh priemman.my.id"}
    };
    for (const auto& [name, description] : kProps) {
        schema.properties->emplace(
            std::string{name},
            userver::yaml_config::SchemaPtr(
                userver::yaml_config::impl::SchemaFromString(
                    "type: string\ndescription: " + std::string{description} + "\n")
            )
        );
    }
}

userver::yaml_config::Schema LogoutHandler::GetStaticConfigSchema() {
    auto schema = userver::server::handlers::HttpHandlerBase::GetStaticConfigSchema();
    AddProperties(schema);
    return schema;
}

LogoutHandler::LogoutHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : userver::server::handlers::HttpHandlerBase(config, context),
      _domain{config["domain"].As<std::string>()},
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
        res.SetContentType(errors::kProtobufContentType);
        return errors::BuildErrorResult("INVALID_REQUEST_BODY");
    }

    const bool revoked = _sessions.Revoke(req.session_token());
    res.SetHeader(std::string("Set-Cookie"), BuildSessionClearCookie(_domain));

    priemman::v1::LogoutResponse response;
    response.set_success(revoked);

    res.SetContentType("application/x-protobuf");
    return response.SerializeAsString();
}

}
