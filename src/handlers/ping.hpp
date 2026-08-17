#pragma once
#include <userver/formats/json/value.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/components/component_context.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/formats/json.hpp>

namespace priemman {

    class PingHandler final : public userver::server::handlers::HttpHandlerBase {
    public:
        static constexpr std::string_view kName = "handler-ping";
        using HttpHandlerBase::HttpHandlerBase;
        std::string HandleRequestThrow(
            const userver::server::http::HttpRequest&,
            userver::server::request::RequestContext&) const override;

    };

}
