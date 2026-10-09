#include <print>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/server/handlers/ping.hpp>
#include <userver/utils/daemon_run.hpp>
#include "components/config_map.hpp"
#include "handlers/register.hpp"
#include "handlers/config.hpp"

int main(int argc, char* argv[]){
    const auto dynamic_server{
        userver::components::MinimalServerComponentList()
        .Append<userver::server::handlers::Ping>()
        .Append<priemman::components::dynamic_config::server::ConfigMapComponent>()
        .Append<priemman::handlers::config::RegisterHandler>()
        .Append<priemman::handlers::config::DynamicConfigHandler>()
    };

    std::println("\n====================================================================");
    std::println(" Priemman Dynamic Server");
    std::println("====================================================================");

    userver::utils::DaemonMain(argc, argv, dynamic_server);
}