#pragma once
#include <src/handlers/user/authenticated_handler_base.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <src/handlers/utils.hpp>
#include <src/handlers/user/proto_convert.hpp>

namespace priemman::handlers::user {

    class PublicProfileHandler final : public AuthenticatedHandlerBase {
    public:

        static constexpr std::string_view kName{"handler-public-profile"};
        PublicProfileHandler(
            const userver::components::ComponentConfig&, 
            const userver::components::ComponentContext&
        );

        std::string HandleRequestThrow(
            const userver::server::http::HttpRequest&,
            userver::server::request::RequestContext&
        ) const override;
    };

}