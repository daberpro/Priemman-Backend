#include "oauth_repository.hpp"

#include <stdexcept>

#include <userver/storages/mysql/cluster_host_type.hpp>
#include <userver/storages/mysql/query.hpp>
#include <userver/utils/uuid4.hpp>

namespace priemman::database {

OAuthRepository::OAuthRepository(
    std::shared_ptr<userver::storages::mysql::Cluster>* mysql_cluster
)
    : _mysql_cluster(*mysql_cluster) {
}

std::optional<OAuthAccount> OAuthRepository::FindByProvider(
    const std::string& provider,
    const std::string& provider_user_id
) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            R"sql(
                SELECT
                    id,
                    user_id,
                    provider,
                    provider_user_id,
                    provider_email,
                    email_verified
                FROM oauth_accounts
                WHERE provider = ?
                  AND provider_user_id = ?
                LIMIT 1
            )sql"
        },
        provider,
        provider_user_id
    ).AsOptionalSingleRow<OAuthAccount>();
}

std::vector<OAuthAccount> OAuthRepository::FindByUser(
    const std::string& user_id
) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            R"sql(
                SELECT
                    id,
                    user_id,
                    provider,
                    provider_user_id,
                    provider_email,
                    email_verified
                FROM oauth_accounts
                WHERE user_id = ?
                ORDER BY created_at ASC
            )sql"
        },
        user_id
    ).AsVector<OAuthAccount>();
}

OAuthAccount OAuthRepository::Create(
    const std::string& user_id,
    const std::string& provider,
    const std::string& provider_user_id,
    const std::string& provider_email,
    bool email_verified
) const {
    const std::string id = userver::utils::generators::GenerateUuid();

        // Cast bool ke std::int8_t untuk MySQL (1 = true, 0 = false)
        const std::int8_t verified_int = email_verified ? 1 : 0;

        _mysql_cluster->Execute(
            userver::storages::mysql::ClusterHostType::kPrimary,
            userver::storages::mysql::Query{
                R"sql(
                    INSERT INTO oauth_accounts (
                        id, user_id, provider, provider_user_id,
                        provider_email, email_verified
                    )
                    VALUES (?, ?, ?, ?, ?, ?)
                    ON DUPLICATE KEY UPDATE
                        provider_email = VALUES(provider_email),
                        email_verified = VALUES(email_verified)
                )sql"
            },
            id,
            user_id,
            provider,
            provider_user_id,
            provider_email,
            verified_int // Pass std::int8_t
        );

        auto account = FindByProviderOnPrimary(provider, provider_user_id);
        if (!account.has_value()) {
            throw std::runtime_error("Failed to upsert OAuth account");
        }
        return *account;
}

bool OAuthRepository::Delete(
    const std::string& provider,
    const std::string& provider_user_id
) const {
    auto result = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                DELETE FROM oauth_accounts
                WHERE provider = ?
                  AND provider_user_id = ?
            )sql"
        },
        provider,
        provider_user_id
    ).AsExecutionResult();

    return result.rows_affected > 0;
}

std::optional<OAuthAccount> OAuthRepository::FindByProviderOnPrimary(
    const std::string& provider,
    const std::string& provider_user_id
) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                SELECT
                    id,
                    user_id,
                    provider,
                    provider_user_id,
                    provider_email,
                    email_verified
                FROM oauth_accounts
                WHERE provider = ?
                  AND provider_user_id = ?
                LIMIT 1
            )sql"
        },
        provider,
        provider_user_id
    ).AsOptionalSingleRow<OAuthAccount>();
}

}  // namespace priemman::database
