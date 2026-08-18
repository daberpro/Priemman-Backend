#pragma once

#include <string>

#include <userver/components/component_base.hpp>
#include <userver/components/loggable_component_base.hpp>
#include <userver/yaml_config/yaml_config.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

#include "CloudinaryClient.hpp"

namespace priemman::cloudinary {

class CloudinaryComponent final : public userver::components::LoggableComponentBase {
public:
    static constexpr std::string_view kName{"cloudinary-client-component"};

    CloudinaryComponent(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );

    // Getter untuk mengakses client
    const Client& GetClient() const { return client_; }

    // Untuk validasi schema
    static userver::yaml_config::Schema GetStaticConfigSchema();

private:
    Client client_;
};

} // namespace priemman::cloudinary
