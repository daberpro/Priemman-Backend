#pragma once

#include <string>

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/http/http_response.hpp>

#include <proto/media.pb.h>
#include <src/handlers/user/authenticated_handler_base.hpp>
#include <src/component/Cloudinary/CloudinaryClientComponent.hpp>

namespace priemman::handlers::media {

class UploadMediaHandler final : public AuthenticatedHandlerBase {
public:
    static constexpr std::string_view kName = "handler-upload-media";

    UploadMediaHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );

    std::string HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;

private:
    const cloudinary::Client& cloudinary_client_;
};

} // namespace priemman::handlers::media
