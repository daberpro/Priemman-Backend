#include "account_repository.hpp"

#include <userver/storages/mysql/cluster_host_type.hpp>
#include <userver/storages/mysql/query.hpp>
#include <userver/utils/uuid4.hpp>

namespace priemman::database {

AccountRepository::AccountRepository(
    std::shared_ptr<userver::storages::mysql::Cluster>* mysql_cluster
)
    : _mysql_cluster(*mysql_cluster) {
}

std::vector<WorkExperienceRow> AccountRepository::ListWorkExperiences(
    const std::string& user_id
) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{R"sql(
            SELECT
                id, title, company, is_current,
                DATE_FORMAT(start_date, '%Y-%m-%dT%H:%i:%sZ'),
                DATE_FORMAT(end_date,   '%Y-%m-%dT%H:%i:%sZ'),
                description
            FROM work_experiences
            WHERE user_id = ?
            ORDER BY start_date DESC
        )sql"},
        user_id
    ).AsVector<WorkExperienceRow>();
}

std::string AccountRepository::UpsertWorkExperience(
    const std::string& user_id,
    const WorkExperienceRow& row
) const {
    if (row.id.empty()) {
        const std::string id = userver::utils::generators::GenerateUuid();
        _mysql_cluster->Execute(
            userver::storages::mysql::ClusterHostType::kPrimary,
            userver::storages::mysql::Query{R"sql(
                INSERT INTO work_experiences (
                    id, user_id, title, company, is_current,
                    start_date, end_date, description
                )
                VALUES (?, ?, ?, ?, ?, ?, ?, ?)
            )sql"},
            id, user_id, row.title, row.company, row.is_current,
            row.start_date, row.end_date, row.description
        );
        return id;
    }

    _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{R"sql(
            UPDATE work_experiences
            SET title = ?, company = ?, is_current = ?,
                start_date = ?, end_date = ?, description = ?
            WHERE id = ? AND user_id = ?
        )sql"},
        row.title, row.company, row.is_current,
        row.start_date, row.end_date, row.description,
        row.id, user_id
    );
    return row.id;
}

bool AccountRepository::DeleteWorkExperience(
    const std::string& user_id,
    const std::string& id
) const {
    auto result = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{R"sql(
            DELETE FROM work_experiences
            WHERE id = ? AND user_id = ?
        )sql"},
        id, user_id
    ).AsExecutionResult();
    return result.rows_affected > 0;
}

std::vector<ConnectedAccountRow> AccountRepository::ListConnectedAccounts(
    const std::string& user_id
) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{R"sql(
            SELECT
                platform, handle_or_url, verified,
                DATE_FORMAT(connected_at, '%Y-%m-%dT%H:%i:%sZ')
            FROM connected_accounts
            WHERE user_id = ?
            ORDER BY connected_at ASC
        )sql"},
        user_id
    ).AsVector<ConnectedAccountRow>();
}

bool AccountRepository::DeleteConnectedAccount(
    const std::string& user_id,
    const std::string& platform
) const {
    auto result = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{R"sql(
            DELETE FROM connected_accounts
            WHERE user_id = ? AND platform = ?
        )sql"},
        user_id, platform
    ).AsExecutionResult();
    return result.rows_affected > 0;
}

bool AccountRepository::UpsertConnectedAccount(
    const std::string& user_id,
    const ConnectedAccountRow& row
) const {
    auto result = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{R"sql(
            INSERT INTO connected_accounts (user_id, platform, handle_or_url, verified, connected_at)
            VALUES (?, ?, ?, ?, ?)
            ON DUPLICATE KEY UPDATE
                handle_or_url = VALUES(handle_or_url),
                verified = VALUES(verified),
                connected_at = VALUES(connected_at)
        )sql"},
        user_id, row.platform, row.handle_or_url, row.verified, row.connected_at
    ).AsExecutionResult();
    return result.rows_affected > 0;
}


}  // namespace priemman::database
