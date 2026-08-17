#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/mysql/cluster.hpp>
#include <userver/storages/mysql/component.hpp>
#include <src/component/SMTP/SMTP.hpp>
#include <src/database/otp_repository.hpp>

namespace priemman::auth {

class SendOtpHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-send-otp";

    SendOtpHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );

    std::string HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;

private:
    static std::string GenerateOtpCode();
    static std::string BuildOtpEmailHtml(const std::string& otp_code);

    std::shared_ptr<userver::storages::mysql::Cluster> _mysql_cluster;
    daberdev::components::SMTPClientComponent* _smtp_component{nullptr};
    database::OtpRepository _otp_repo;
};

}  // namespace priemman::auth
