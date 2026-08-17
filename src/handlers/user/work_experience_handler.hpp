#pragma once

#include <src/handlers/user/authenticated_handler_base.hpp>

namespace priemman::handlers::user {

class WorkExperienceHandler final : public AuthenticatedHandlerBase {
public:
    static constexpr std::string_view kName = "handler-work-experience";

    using AuthenticatedHandlerBase::AuthenticatedHandlerBase;

    std::string HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;
};

}  // namespace priemman::handlers::user
