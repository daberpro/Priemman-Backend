#include <print>

#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/client.hpp>
#include <userver/clients/http/component_core.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/logging/component.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/mysql.hpp>
#include <userver/storages/mysql/component.hpp>
#include <userver/storages/secdist/component.hpp>
#include <userver/storages/secdist/provider_component.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/clients/http/middlewares/pipeline_component.hpp>
#include <userver/clients/http/client_core.hpp>
#include <userver/utils/daemon_run.hpp>

#include <src/auth/send_otp_handler.hpp>
#include <src/auth/oauth_initiate_handler.hpp>
#include <src/auth/oauth_callback_handler.hpp>
#include <src/ping.hpp>

#include <src/component/SMTP/SMTP.hpp>
#include <src/component/OAuth/Google/OAuthGoogleComponent.hpp>
#include <src/component/OAuth/Github/OAuthGithubComponent.hpp>

auto main(int argc, char* argv[]) -> int {
    auto component_list = userver::components::MinimalServerComponentList()
        .Append<userver::storages::mysql::Component>("database")
        .Append<userver::components::Secdist>()
        .Append<userver::components::DefaultSecdistProvider>()
        .Append<userver::components::HttpClient>()
        .Append<userver::components::HttpClientCore>()
        .Append<userver::clients::dns::Component>()
        .Append<userver::clients::http::MiddlewarePipelineComponent>()

        .Append<daberdev::components::SMTPClientComponent>("daberdev-smtp-component-client")
        .Append<daberdev::components::OAuthGoogleComponent>("daberdev-oauth-google-component")
        .Append<daberdev::components::OAuthGithubComponent>("daberdev-oauth-github-component")

        .Append<priemman::auth::SendOtpHandler>("handler-send-otp")
        .Append<priemman::auth::InitiateGoogleOAuthHandler>("handler-initiate-google-oauth")
        .Append<priemman::auth::OAuthGoogleCallbackHandler>("handler-callback-google-oauth")
        .Append<priemman::auth::InitiateGithubOAuthHandler>("handler-initiate-github-oauth")
        .Append<priemman::auth::OAuthGithubCallbackHandler>("handler-callback-github-oauth")
        .Append<priemman::PingHandler>("handler-ping");

    std::println("=========================================");
    std::println(" Priemman Backend Server");
    std::println(" Starting on config: {}",
                  argc > 2 ? argv[2] : "../config/config.yaml");
    std::println("=========================================");

    return userver::utils::DaemonMain(argc, argv, component_list);
}
