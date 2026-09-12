#pragma once

#include <string>
#include <string_view>
#include <userver/clients/http/component.hpp>
#include <vector>

#include <userver/clients/http/client.hpp>
#include <src/handlers/user/authenticated_handler_base.hpp>

namespace priemman::handlers::workspace {

struct CalendarEvent {
    std::string title;
    std::string description;
    std::string start;
    std::string end;
    std::string location;
};

class CalendarHandler final
    : public AuthenticatedHandlerBase {

public:

    static constexpr std::string_view kName = "handler-calendar-workspace";

    CalendarHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    ) : AuthenticatedHandlerBase(config,context) ,
    _client{&context.FindComponent<userver::components::HttpClient>().GetHttpClient()},
    _workspace_host{config["workspace_host"].As<std::string>()},
    _username{config["username"].As<std::string>()},
    _password{config["password"].As<std::string>()}
    {};

    static userver::yaml_config::Schema GetStaticConfigSchema();

    std::string HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;



private:

    std::vector<CalendarEvent> ParseIcsEvents(
        std::string_view ics_data
    ) const;

    userver::clients::http::Client* _client;

    std::string _workspace_host;
    std::string _username;
    std::string _password;

};

} // namespace priemman::handlers::workspace
