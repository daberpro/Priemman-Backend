#pragma once
#include <userver/formats/json/value.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/components/component_context.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/storages/mysql.hpp>
#include <userver/storages/mysql/component.hpp>
#include <userver/formats/json.hpp>
#include <src/component/SMTP/SMTP.hpp>

namespace priemman::auth {

    class SendOtpHandler final : public userver::server::handlers::HttpHandlerBase {
    public:
        static constexpr std::string_view kName = "handler-send-otp";
        SendOtpHandler(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context)
        : userver::server::handlers::HttpHandlerBase(config, context),
          _mysql_component(&context.FindComponent<userver::storages::mysql::Component>("database")),
          _smtp_component(&context.FindComponent<daberdev::components::SMTPClientComponent>("daberdev-smtp-component-client")) {};

        std::string HandleRequestThrow(
            const userver::server::http::HttpRequest& request,
            userver::server::request::RequestContext& context) const override;

    private:
        static std::string GenerateOtpCode();
        static std::string BuildOtpEmailHtml(const std::string& otp_code);

        userver::storages::mysql::Component* _mysql_component{nullptr};
        daberdev::components::SMTPClientComponent* _smtp_component{nullptr};

    };

}
