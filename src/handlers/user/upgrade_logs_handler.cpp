#include "upgrade_logs_handler.hpp"

namespace priemman::handlers::user {

UpgradeLogsHandler::UpgradeLogsHandler(
    const userver::components::ComponentConfig &config,
    const userver::components::ComponentContext &context)
    : AuthenticatedHandlerBase(config, context),
      _creator_upgrade_logs(database::CreatorUpgradeLogs(&_mysql_cluster)) {}

std::string UpgradeLogsHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest &request,
    userver::server::request::RequestContext &) const {

  const auto limit = priemman::utils::ParsePageSize(request);
  const auto offset = priemman::utils::ParseOffset(request);

  auto user_id = TryAuth(request);
  if (!user_id.has_value()) {
    return ErrorResult("UNAUTHORIZED", "Missing or invalid session token");
  }

  auto logs = _creator_upgrade_logs.GetAllLogs(user_id.value(), limit, offset);
  auto result = mapper::ToProto(logs);
  return result.SerializePartialAsString();
}

} // namespace priemman::handlers::user