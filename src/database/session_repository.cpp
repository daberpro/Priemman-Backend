#include "session_repository.hpp"

#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <openssl/rand.h>
#include <openssl/sha.h>

#include <userver/storages/mysql/cluster_host_type.hpp>
#include <userver/storages/mysql/query.hpp>
#include <userver/utils/uuid4.hpp>

namespace priemman::database {

namespace {

std::string GenerateToken() {
    unsigned char bytes[32];

    if (RAND_bytes(bytes, sizeof(bytes)) != 1) {
        throw std::runtime_error("Failed to generate session token");
    }

    std::ostringstream stream;

    for (const auto byte : bytes) {
        stream << std::hex
               << std::setw(2)
               << std::setfill('0')
               << static_cast<int>(byte);
    }

    return stream.str();
}

std::string HashToken(
    const std::string& token
) {
    unsigned char hash[SHA256_DIGEST_LENGTH];

    SHA256(
        reinterpret_cast<const unsigned char*>(token.data()),
        token.size(),
        hash
    );

    std::ostringstream stream;

    for (const auto byte : hash) {
        stream << std::hex
               << std::setw(2)
               << std::setfill('0')
               << static_cast<int>(byte);
    }

    return stream.str();
}

}  // namespace

SessionRepository::SessionRepository(
    std::shared_ptr<userver::storages::mysql::Cluster>* mysql_cluster
)
    : _mysql_cluster(*mysql_cluster) {
}

Session SessionRepository::Create(
    const std::string& user_id
) const {
    const std::string token = GenerateToken();
    const std::string token_hash = HashToken(token);
    const std::string id = userver::utils::generators::GenerateUuid();

    _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                INSERT INTO sessions (
                    id,
                    token_hash,
                    user_id,
                    expires_at
                )
                VALUES (
                    ?,
                    ?,
                    ?,
                    DATE_ADD(NOW(6), INTERVAL 30 DAY)
                )
            )sql"
        },
        id,
        token_hash,
        user_id
    );

    return {
        .id = id,
        .token = token,
        .user_id = user_id
    };
}

bool SessionRepository::Revoke(
    const std::string& token
) const {
    const std::string token_hash = HashToken(token);

    auto result = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                UPDATE sessions
                SET revoked_at = NOW(6)
                WHERE token_hash = ?
                  AND revoked_at IS NULL
            )sql"
        },
        token_hash
    ).AsExecutionResult();

    return result.rows_affected > 0;
}

std::optional<std::string> SessionRepository::FindUserIdByToken(
    const std::string& token
) const {
    const std::string token_hash = HashToken(token);

    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            R"sql(
                SELECT user_id
                FROM sessions
                WHERE token_hash = ?
                  AND revoked_at IS NULL
                  AND expires_at > NOW(6)
                LIMIT 1
            )sql"
        },
        token_hash
    ).AsOptionalSingleField<std::string>();
}

}  // namespace priemman::database
