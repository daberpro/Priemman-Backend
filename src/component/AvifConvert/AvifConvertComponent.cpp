#include "AvifConvertComponent.hpp"
#include <vips/vips8>
#include <userver/engine/async.hpp>

namespace daberdev::components {

AvifConvertComponent::AvifConvertComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : userver::components::LoggableComponentBase(config, context),
      _fs_task_processor{
          &context.GetTaskProcessor("fs-task-processor")
      },
      _quality{
          config["quality"].As<int>()
      },
      _lossless{
          config["lossless"].As<bool>()
      },
      _effort{
          config["effort"].As<int>()
      } {
    if (VIPS_INIT("server")) {
        throw std::runtime_error("Failed to initialize VIPS");
    }
    // Force libvips plugins to initialize while
    // component construction is still allowed to dlopen().
    vips_foreign_find_load_buffer(nullptr, 0);
}

AvifConvertComponent::~AvifConvertComponent(){
    vips_shutdown();
}

std::expected<std::string, std::string>
AvifConvertComponent::ConvertBuffer(
    const std::string& buffer
) const {
    try {
        vips::VImage image{
            vips::VImage::new_from_buffer(
                buffer.data(),
                buffer.size(),
                "",
                vips::VImage::option()
            )
        };

        void* out_buffer{nullptr};
        size_t size_buffer{0};

        image.write_to_buffer(
            ".avif",
            &out_buffer,
            &size_buffer,
            vips::VImage::option()
                ->set("Q", _quality)
                ->set("lossless", _lossless)
                ->set("effort", _effort)
        );

        std::string result_buffer{
            static_cast<char*>(out_buffer),
            size_buffer
        };

        g_free(out_buffer);

        return result_buffer;
    } catch (const std::exception& err) {
        return std::unexpected<std::string>(err.what());
    }
}

std::expected<std::string, std::string>
AvifConvertComponent::ConvertBufferAsync(
    const std::string& buffer
) const {
    return userver::engine::AsyncNoTracing(
        *_fs_task_processor,
        [this, &buffer]() {
            return ConvertBuffer(buffer);
        }
    ).Get();
}

userver::yaml_config::Schema
AvifConvertComponent::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<
        userver::components::LoggableComponentBase
    >(R"(
        type: object
        description: AVIF image conversion component
        additionalProperties: false

        properties:
            quality:
                type: integer
                description: AVIF image quality from 0 to 100
                default: 55
                minimum: 0
                maximum: 100

            lossless:
                type: boolean
                description: Encode AVIF images losslessly
                default: false

            effort:
                type: integer
                description: AVIF encoder effort level from 0 to 9
                default: 4
                minimum: 0
                maximum: 9
    )");
}

}  // namespace daberdev::avifconvert