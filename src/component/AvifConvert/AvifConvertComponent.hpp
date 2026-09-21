#pragma once 

#include <string>
#include <vips/vips8>
#include <userver/components/component_base.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/components/loggable_component_base.hpp>
#include <userver/engine/async.hpp>
#include <userver/concurrent/background_task_storage.hpp>
#include <userver/yaml_config/yaml_config.hpp>
#include <userver/yaml_config/merge_schemas.hpp>
#include <expected>

namespace daberdev::components {

    class AvifConvertComponent final : public userver::components::LoggableComponentBase {
    public:

        static constexpr std::string_view kName{"avifconvert-component"};
        AvifConvertComponent(const userver::components::ComponentConfig&,const userver::components::ComponentContext&);
        ~AvifConvertComponent();

        // void ConvertFile(const std::string& file_path);
        // void ConvertFileAsync(const std::string& file_path);
        std::expected<std::string, std::string> ConvertBuffer(const std::string& buffer) const;
        std::expected<std::string, std::string> ConvertBufferAsync(const std::string& buffer) const;

        static userver::yaml_config::Schema GetStaticConfigSchema();

    private:

        userver::engine::TaskProcessor* _fs_task_processor{nullptr};
        int _quality;
        bool _lossless;
        int _effort;

    };

}