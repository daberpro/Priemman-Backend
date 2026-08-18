#include "CloudinaryClientComponent.hpp"

#include <userver/clients/http/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>

namespace priemman::cloudinary {

CloudinaryComponent::CloudinaryComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : LoggableComponentBase(config, context),
      client_(
          &context.FindComponent<userver::components::HttpClient>().GetHttpClient(),
          config["cloud_name"].As<std::string>(),
          config["api_key"].As<std::string>(),
          config["api_secret"].As<std::string>()
      ) {}

userver::yaml_config::Schema CloudinaryComponent::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<userver::components::LoggableComponentBase>(R"(
type: object
description: Cloudinary client component for media management
additionalProperties: false
properties:
    cloud_name:
        type: string
        description: Cloudinary cloud name
    api_key:
        type: string
        description: Cloudinary API key
    api_secret:
        type: string
        description: Cloudinary API secret
)");
}

} // namespace priemman::cloudinary
