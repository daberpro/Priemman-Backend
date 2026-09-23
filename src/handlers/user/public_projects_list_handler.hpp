#pragma once
#include <src/handlers/user/authenticated_handler_base.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <src/handlers/utils.hpp>
#include <src/handlers/user/proto_convert.hpp>
#include <src/database/project_repository.hpp>
#include <cstdlib>
#include <string>
#include <proto/project.pb.h>
#include <userver/server/http/http_method.hpp>
#include <src/handlers/projects/project_proto_convert.hpp>
#include <src/handlers/utils.hpp>


namespace priemman::handlers::user {

    class PublicProjectsListHandler final : public AuthenticatedHandlerBase {
    public:

        static constexpr std::string_view kName{"handler-public-projects-list"};
        PublicProjectsListHandler(
            const userver::components::ComponentConfig&, 
            const userver::components::ComponentContext&
        );

        std::string HandleRequestThrow(
            const userver::server::http::HttpRequest&,
            userver::server::request::RequestContext&
        ) const override;
    };

}