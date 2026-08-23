#include <print>

#include <userver/engine/io/common.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/components/fs_cache.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/storages/mysql.hpp>
#include <userver/storages/mysql/component.hpp>
#include <userver/storages/secdist/component.hpp>
#include <userver/storages/secdist/provider_component.hpp>
#include <userver/utils/daemon_run.hpp>

#include <frontend/server/session_middleware.hpp>
#include <frontend/server/static_page_handler.hpp>
#include <src/middleware/rate_limiter.hpp>

auto main(int argc, char* argv[]) -> int {
    auto component_list = userver::components::MinimalServerComponentList()
        .Append<userver::clients::dns::Component>()
        .Append<userver::storages::mysql::Component>("database")
        .Append<userver::components::Secdist>()
        .Append<userver::components::DefaultSecdistProvider>()
        .Append<priemman::middlewares::RateLimiterFactory>()
        .Append<priemman::frontend::SessionMiddlewareFactory>()
        .Append<userver::components::FsCache>("fs-cache-web")
        .Append<priemman::frontend::StaticPageHandler>("handler-pages");

    std::println("=========================================");
    std::println(" Priemman Frontend Server");
    std::println("=========================================");

    return userver::utils::DaemonMain(argc, argv, component_list);
}
