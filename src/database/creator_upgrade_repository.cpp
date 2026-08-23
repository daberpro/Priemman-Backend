#include "creator_upgrade_repository.hpp"

#include <stdexcept>

#include <userver/storages/mysql/cluster_host_type.hpp>
#include <userver/storages/mysql/query.hpp>
#include <userver/utils/uuid4.hpp>

namespace priemman::database {

namespace {

constexpr std::string_view kSelectUpgradeRow = R"sql(
    SELECT id, user_id, status, invoice_id, invoice_amount, currency,
           rejection_reason,
           DATE_FORMAT(requested_at, '%Y-%m-%dT%H:%i:%sZ'),
           DATE_FORMAT(reviewed_at,  '%Y-%m-%dT%H:%i:%sZ'),
           DATE_FORMAT(paid_at,      '%Y-%m-%dT%H:%i:%sZ')
    FROM creator_upgrades
)sql";

constexpr std::string_view kSelectRequestWithUser = R"sql(
    SELECT cu.id, cu.user_id, u.email, cu.status, cu.invoice_id,
           cu.invoice_amount, cu.currency, cu.rejection_reason,
           DATE_FORMAT(cu.requested_at, '%Y-%m-%dT%H:%i:%sZ')
    FROM creator_upgrades cu
    INNER JOIN users u ON u.id = cu.user_id
)sql";

void ValidateStatus(const std::string& status) {
    if (status != "pending" && status != "approved" &&
        status != "rejected" && status != "paid") {
        throw std::invalid_argument("Invalid upgrade status: " + status);
    }
}

struct UserIdStatusRow {
    std::string user_id;
    std::string status;
};

}  // namespace

CreatorUpgradeRepository::CreatorUpgradeRepository(
    std::shared_ptr<userver::storages::mysql::Cluster>* mysql_cluster
)
    : _mysql_cluster(*mysql_cluster) {
}

bool CreatorUpgradeRepository::CreateRequest(const std::string& user_id) const {
    const auto active = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                SELECT 1 FROM creator_upgrades
                WHERE user_id = ? AND status IN ('pending', 'approved')
                LIMIT 1
            )sql"
        },
        user_id
    ).AsOptionalSingleField<std::int32_t>();

    if (active.has_value()) {
        return false;
    }

    const auto result = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                INSERT INTO creator_upgrades (id, user_id, status)
                VALUES (?, ?, 'pending')
            )sql"
        },
        userver::utils::generators::GenerateUuid(),
        user_id
    ).AsExecutionResult();

    return result.rows_affected > 0;
}

std::optional<UpgradeRow> CreatorUpgradeRepository::FindActiveByUser(
    const std::string& user_id
) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            std::string{kSelectUpgradeRow} +
            " WHERE user_id = ? AND status IN ('pending', 'approved')"
            " ORDER BY requested_at DESC LIMIT 1"
        },
        user_id
    ).AsOptionalSingleRow<UpgradeRow>();
}

std::optional<UpgradeRow> CreatorUpgradeRepository::FindLatestByUser(
    const std::string& user_id
) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            std::string{kSelectUpgradeRow} +
            " WHERE user_id = ? ORDER BY requested_at DESC LIMIT 1"
        },
        user_id
    ).AsOptionalSingleRow<UpgradeRow>();
}

std::optional<UpgradeRequestRow> CreatorUpgradeRepository::FindByIdWithUser(
    const std::string& id
) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            std::string{kSelectRequestWithUser} + " WHERE cu.id = ? LIMIT 1"
        },
        id
    ).AsOptionalSingleRow<UpgradeRequestRow>();
}

std::vector<UpgradeRequestRow> CreatorUpgradeRepository::ListByStatus(
    const std::string& status
) const {
    ValidateStatus(status);

    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            std::string{kSelectRequestWithUser} +
            " WHERE cu.status = '" + status + "'"
            " ORDER BY cu.requested_at DESC LIMIT 100"
        }
    ).AsVector<UpgradeRequestRow>();
}

bool CreatorUpgradeRepository::Approve(const std::string& id) const {
    const auto result = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                UPDATE creator_upgrades
                SET status = 'approved',
                    invoice_id = ?,
                    invoice_amount = ?,
                    reviewed_at = NOW(6)
                WHERE id = ? AND status = 'pending'
            )sql"
        },
        userver::utils::generators::GenerateUuid(),
        kCreatorUpgradePrice,
        id
    ).AsExecutionResult();

    return result.rows_affected > 0;
}

bool CreatorUpgradeRepository::Reject(
    const std::string& id,
    const std::string& reason
) const {
    const auto result = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                UPDATE creator_upgrades
                SET status = 'rejected',
                    rejection_reason = ?,
                    reviewed_at = NOW(6)
                WHERE id = ? AND status = 'pending'
            )sql"
        },
        reason, id
    ).AsExecutionResult();

    return result.rows_affected > 0;
}

std::optional<std::string> CreatorUpgradeRepository::ConfirmPaid(
    const std::string& id
) const {
    const auto row = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            "SELECT user_id, status FROM creator_upgrades WHERE id = ? LIMIT 1"
        },
        id
    ).AsOptionalSingleRow<UserIdStatusRow>();

    if (!row.has_value() || row->status != "approved") {
        return std::nullopt;
    }

    const auto result = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                UPDATE creator_upgrades
                SET status = 'paid', paid_at = NOW(6)
                WHERE id = ? AND status = 'approved'
            )sql"
        },
        id
    ).AsExecutionResult();

    if (result.rows_affected == 0) {
        return std::nullopt;
    }
    return row->user_id;
}

}  // namespace priemman::database
