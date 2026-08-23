#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <userver/storages/mysql/cluster.hpp>

namespace priemman::database {

inline constexpr std::int64_t kCreatorUpgradePrice = 100000;

struct UpgradeRow {
    std::string id;
    std::string user_id;
    std::string status;
    std::optional<std::string> invoice_id;
    std::optional<std::int64_t> invoice_amount;
    std::string currency;
    std::string rejection_reason;
    std::optional<std::string> requested_at;
    std::optional<std::string> reviewed_at;
    std::optional<std::string> paid_at;
};

struct UpgradeRequestRow {
    std::string id;
    std::string user_id;
    std::string email;
    std::string status;
    std::optional<std::string> invoice_id;
    std::optional<std::int64_t> invoice_amount;
    std::string currency;
    std::string rejection_reason;
    std::optional<std::string> requested_at;
};

class CreatorUpgradeRepository {
public:
    explicit CreatorUpgradeRepository(
        std::shared_ptr<userver::storages::mysql::Cluster>* mysql_cluster
    );

    bool CreateRequest(const std::string& user_id) const;

    std::optional<UpgradeRow> FindActiveByUser(const std::string& user_id) const;

    std::optional<UpgradeRow> FindLatestByUser(const std::string& user_id) const;

    std::optional<UpgradeRequestRow> FindByIdWithUser(const std::string& id) const;

    std::vector<UpgradeRequestRow> ListByStatus(const std::string& status) const;

    bool Approve(const std::string& id) const;

    bool Reject(const std::string& id, const std::string& reason) const;

    std::optional<std::string> ConfirmPaid(const std::string& id) const;

private:
    std::shared_ptr<userver::storages::mysql::Cluster> _mysql_cluster{nullptr};
};

}  // namespace priemman::database
