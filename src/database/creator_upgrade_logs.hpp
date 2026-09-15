#pragma once

#include <vector>
#include <userver/storages/mysql/cluster.hpp>

namespace priemman::database {

    struct UpgradeLog {
        std::string id;
        std::string status;
        std::string rejection_reason;
        std::optional<std::string> requested_at;
        std::optional<std::string> reviewed_at;
    };

    class CreatorUpgradeLogs final {
    public:

        explicit CreatorUpgradeLogs(std::shared_ptr<userver::storages::mysql::Cluster>* mysql_cluster);

        std::vector<UpgradeLog> GetAllLogs(const std::string& user_id, const std::int64_t& limit, const std::int64_t& offset) const;
        
    private:

    std::shared_ptr<userver::storages::mysql::Cluster> _mysql_cluster{nullptr};

    };

}