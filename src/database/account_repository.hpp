#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <userver/storages/mysql/cluster.hpp>

namespace priemman::database {

struct WorkExperienceRow {
    std::string id;
    std::string title;
    std::string company;
    std::int8_t is_current;
    std::optional<std::string> start_date;  // RFC3339 dari DATE_FORMAT
    std::optional<std::string> end_date;
    std::string description;
};

struct ConnectedAccountRow {
    std::string platform;
    std::string handle_or_url;
    std::int8_t verified;
    std::optional<std::string> connected_at;
};

class AccountRepository {
public:
    explicit AccountRepository(
        std::shared_ptr<userver::storages::mysql::Cluster>* mysql_cluster
    );

    std::vector<WorkExperienceRow> ListWorkExperiences(const std::string& user_id) const;
    std::string UpsertWorkExperience(const std::string& user_id, const WorkExperienceRow& row) const;
    bool DeleteWorkExperience(const std::string& user_id, const std::string& id) const;

    std::vector<ConnectedAccountRow> ListConnectedAccounts(const std::string& user_id) const;
    bool DeleteConnectedAccount(const std::string& user_id, const std::string& platform) const;

    bool UpsertConnectedAccount(const std::string& user_id, const ConnectedAccountRow& row) const;

private:
    std::shared_ptr<userver::storages::mysql::Cluster> _mysql_cluster{nullptr};
};

}  // namespace priemman::database
