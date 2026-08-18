#pragma once
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/server/http/http_request.hpp>

namespace priemman {

    class ApiInfoHandler final : public userver::server::handlers::HttpHandlerBase {
    public:
        static constexpr std::string_view kName = "handler-api-info";
        using HttpHandlerBase::HttpHandlerBase;
        std::string HandleRequestThrow(
            const userver::server::http::HttpRequest&,
            userver::server::request::RequestContext&) const override;

    };

}
