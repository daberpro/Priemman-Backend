#pragma once

#include <src/handlers/user/authenticated_handler_base.hpp>
#include <src/database/creator_upgrade_logs.hpp>
#include <src/handlers/user/proto_convert.hpp>
#include <src/handlers/utils.hpp>

namespace priemman::handlers::user {

    class UpgradeLogsHandler final : public AuthenticatedHandlerBase {
    public:

        static constexpr std::string_view kName{"handler-upgrade-logs"};

        UpgradeLogsHandler(
            const userver::components::ComponentConfig&,
            const userver::components::ComponentContext&
        );

        std::string HandleRequestThrow(
            const userver::server::http::HttpRequest&,
            userver::server::request::RequestContext&
        ) const override;

    private:

        database::CreatorUpgradeLogs _creator_upgrade_logs{nullptr};

    };

}

