#include "creator_upgrade_logs.hpp"

namespace priemman::database {

CreatorUpgradeLogs::CreatorUpgradeLogs(
    std::shared_ptr<userver::storages::mysql::Cluster> *mysql_cluster)
    : _mysql_cluster{*mysql_cluster} {}

std::vector<UpgradeLog>
CreatorUpgradeLogs::GetAllLogs(const std::string &user_id,
                               const std::int64_t &limit,
                               const std::int64_t &offset) const {
  try {
    return _mysql_cluster->Execute(userver::storages::mysql::ClusterHostType::kSecondary,
                  userver::storages::mysql::Query{
                      R"sql(
                    SELECT
                        id,
                        status,
                        rejection_reason,
                        DATE_FORMAT(
                            requested_at,
                            '%Y-%m-%dT%H:%i:%sZ'
                        ),
                        DATE_FORMAT(
                            reviewed_at,
                            '%Y-%m-%dT%H:%i:%sZ'
                        )
                    FROM upgrade_logs
                    WHERE user_id = ?
                    ORDER BY requested_at DESC
                    LIMIT ?
                    OFFSET ?
                )sql"},
                  user_id, limit, offset)
        .AsVector<UpgradeLog>();

  } catch (const std::exception &err) {
    LOG_ERROR() << "Failed to get upgrade logs: " << err.what();
    return {};
  }
}

} // namespace priemman::database