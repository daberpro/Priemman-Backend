#pragma once

#include <memory>
#include <optional>
#include <string>

#include <userver/storages/mysql/cluster.hpp>

namespace priemman::database {

struct Session {
    std::string id;
    std::string token;
    std::string user_id;
};

struct SessionIdentity {
    std::string user_id;
    std::string role;
};

class SessionRepository {
public:
    explicit SessionRepository(
        std::shared_ptr<userver::storages::mysql::Cluster>* mysql_cluster
    );

    Session Create(
        const std::string& user_id
    ) const;

    bool Revoke(
        const std::string& token
    ) const;

    std::optional<std::string> FindUserIdByToken(
        const std::string& token
    ) const;

    std::optional<SessionIdentity> FindIdentityByToken(
        const std::string& token
    ) const;

private:
    std::shared_ptr<userver::storages::mysql::Cluster> _mysql_cluster{nullptr};
};

}  // namespace priemman::database
