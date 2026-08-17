#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <userver/storages/mysql/cluster.hpp>

namespace priemman::database {

struct OAuthAccount {
    std::string id;
    std::string user_id;
    std::string provider;
    std::string provider_user_id;
    std::string provider_email;
    int8_t email_verified;
};

class OAuthRepository {
public:
    explicit OAuthRepository(
        std::shared_ptr<userver::storages::mysql::Cluster>* mysql_cluster
    );

    std::optional<OAuthAccount> FindByProvider(
        const std::string& provider,
        const std::string& provider_user_id
    ) const;

    std::vector<OAuthAccount> FindByUser(
        const std::string& user_id
    ) const;

    // Idempotent lewat unique key (provider, provider_user_id).
    OAuthAccount Create(
        const std::string& user_id,
        const std::string& provider,
        const std::string& provider_user_id,
        const std::string& provider_email,
        bool email_verified
    ) const;

    bool Delete(
        const std::string& provider,
        const std::string& provider_user_id
    ) const;

private:
    std::optional<OAuthAccount> FindByProviderOnPrimary(
        const std::string& provider,
        const std::string& provider_user_id
    ) const;

    std::shared_ptr<userver::storages::mysql::Cluster> _mysql_cluster{nullptr};
};

}  // namespace priemman::database
