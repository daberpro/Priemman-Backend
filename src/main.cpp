#include <print>

// WORKAROUND(userver 3.1): engine/io/common.hpp harus diproses sebelum header yang
// membuka namespace engine::io::impl (mis. components/fs_cache.hpp via inotify.hpp),
// jika tidak lookup impl::AwaitableBase di common.hpp gagal terkompilasi.
#include <userver/engine/io/common.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/client.hpp>
#include <userver/clients/http/component_core.hpp>
#include <userver/components/fs_cache.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/logging/component.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_static.hpp>
#include <userver/storages/mysql.hpp>
#include <userver/storages/mysql/component.hpp>
#include <userver/storages/secdist/component.hpp>
#include <userver/storages/secdist/provider_component.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/clients/http/middlewares/pipeline_component.hpp>
#include <userver/clients/http/client_core.hpp>
#include <userver/utils/daemon_run.hpp>
#include <userver/server/middlewares/cors.hpp>

#include <src/handlers/auth/send_otp_handler.hpp>
#include <src/handlers/auth/verify_otp_handler.hpp>
#include <src/handlers/auth/logout_handler.hpp>
#include <src/handlers/auth/oauth_initiate_handler.hpp>
#include <src/handlers/auth/oauth_callback_handler.hpp>
#include <src/handlers/ping.hpp>
#include <src/handlers/api_info_handler.hpp>
#include <src/handlers/user/basic_info_handler.hpp>
#include <src/handlers/user/connected_accounts_handler.hpp>
#include <src/handlers/user/work_experience_handler.hpp>
#include <src/handlers/projects/collection_handler.hpp>
#include <src/handlers/projects/project_create_handler.hpp>
#include <src/handlers/projects/project_detail_handler.hpp>
#include <src/handlers/projects/project_list_handler.hpp>
#include <src/handlers/media/upload_media_handler.hpp>
#include <src/component/Cloudinary/CloudinaryClientComponent.hpp>
#include <src/component/Cloudinary/MediaSweeperComponent.hpp>

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
        .Append<userver::server::middlewares::CorsFactory>()
        .Append<userver::components::FsCache>("fs-cache-static")
        .Append<userver::server::handlers::HttpHandlerStatic>("handler-static")

        .Append<daberdev::components::SMTPClientComponent>("daberdev-smtp-component-client")
        .Append<daberdev::components::OAuthGoogleComponent>("daberdev-oauth-google-component")
        .Append<daberdev::components::OAuthGithubComponent>("daberdev-oauth-github-component")
        .Append<priemman::cloudinary::CloudinaryComponent>("cloudinary-client-component")
        .Append<priemman::cloudinary::MediaSweeperComponent>("media-sweeper")

        // Auth Handlers
        .Append<priemman::auth::SendOtpHandler>("handler-send-otp")
        .Append<priemman::auth::VerifyOtpHandler>("handler-verify-otp")
        .Append<priemman::auth::LogoutHandler>("handler-logout")
        .Append<priemman::auth::InitiateGoogleOAuthHandler>("handler-initiate-google-oauth")
        .Append<priemman::auth::OAuthGoogleCallbackHandler>("handler-callback-google-oauth")
        .Append<priemman::auth::InitiateGithubOAuthHandler>("handler-initiate-github-oauth")
        .Append<priemman::auth::OAuthGithubCallbackHandler>("handler-callback-github-oauth")

        // Project Handlers
        .Append<priemman::handlers::projects::ProjectCreateHandler>()
        .Append<priemman::handlers::projects::ProjectListHandler>()
        .Append<priemman::handlers::projects::ProjectDetailHandler>()
        .Append<priemman::handlers::projects::CollectionHandler>()

        // Media Handlers
        .Append<priemman::handlers::media::UploadMediaHandler>()

        // User Handlers
        .Append<priemman::handlers::user::BasicInfoHandler>()
        .Append<priemman::handlers::user::WorkExperienceHandler>()
        .Append<priemman::handlers::user::ConnectedAccountsHandler>()
        .Append<priemman::ApiInfoHandler>("handler-api-info")
        .Append<priemman::PingHandler>("handler-ping");

    std::println("=========================================");
    std::println(" Priemman Backend Server");
    std::println(" Starting on config: {}", argc > 2 ? argv[2] : "../config/config.yaml");
    std::println("=========================================");

    return userver::utils::DaemonMain(argc, argv, component_list);
}
