#pragma once
#include <cstdint>
#include <userver/components/component_base.hpp>
#include <userver/components/loggable_component_base.hpp>
#include <userver/concurrent/background_task_storage.hpp>
#include <userver/logging/log.hpp>
#include <userver/crypto/crypto.hpp>
#include <userver/yaml_config/yaml_config.hpp>
#include <userver/yaml_config/merge_schemas.hpp>
#include <userver/engine/io/tls_wrapper.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/clients/dns/component.hpp>
#include <userver/clients/dns/resolver.hpp>

namespace daberdev::components {
    class SMTPClientComponent final : public userver::components::LoggableComponentBase {
    public:
        static constexpr std::string_view kName{"daberdev-smtp-component-client"};
        SMTPClientComponent(
            const userver::components::ComponentConfig& config,
            const userver::components::ComponentContext& context
        );

        bool SendEmail(
            const std::string& to,
            const std::string& subject,
            const std::string& body
        );

        void SendEmailAsync(
            std::string to,
            std::string subject,
            std::string body
        );

        static userver::yaml_config::Schema GetStaticConfigSchema();

    private:
        std::string m_host{};
        uint16_t m_port{};
        std::string m_email{};
        std::string m_password{};
        userver::clients::dns::Resolver* _resolver;
        userver::concurrent::BackgroundTaskStorage m_background_tasks;

    };
}
