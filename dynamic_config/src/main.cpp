#include <userver/components/minimal_server_component_list.hpp>
#include <userver/server/handlers/ping.hpp>
#include <userver/utils/daemon_run.hpp>

int main(
    [[maybe_unused]] int argc,
    [[maybe_unused]] char* argv[]
){
    const auto server{ 
        userver::components::MinimalServerComponentList() 
        .Append<userver::server::handlers::Ping>()
    };

    return userver::utils::DaemonMain(argc, argv,server);
}