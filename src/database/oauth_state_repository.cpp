#include "oauth_state_repository.hpp"
#include <cstdint>
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

std::string GenerateState() {
    unsigned char bytes[32];

    if (RAND_bytes(bytes, sizeof(bytes)) != 1) {
        throw std::runtime_error("Failed to generate OAuth state");
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

std::string HashState(
    const std::string& state
) {
    unsigned char hash[SHA256_DIGEST_LENGTH];

    SHA256(
        reinterpret_cast<const unsigned char*>(state.data()),
        state.size(),
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

OAuthStateRepository::OAuthStateRepository(
    std::shared_ptr<userver::storages::mysql::Cluster>* mysql_cluster
)
    : _mysql_cluster(*mysql_cluster) {
}

std::string OAuthStateRepository::Create(
    const std::string& provider,
    const std::string& redirect_uri,
    std::chrono::seconds ttl
) const {
    if (provider != "GOOGLE" && provider != "GITHUB") {
        throw std::invalid_argument("Unsupported OAuth provider");
    }

    if (redirect_uri.empty()) {
        throw std::invalid_argument("redirect_uri must not be empty");
    }

    const std::string state = GenerateState();
    const std::string state_hash = HashState(state);
    const std::string id = userver::utils::generators::GenerateUuid();

    _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                INSERT INTO oauth_states (
                    id,
                    state_hash,
                    provider,
                    redirect_uri,
                    expires_at
                )
                VALUES (
                    ?,
                    ?,
                    ?,
                    ?,
                    TIMESTAMPADD(SECOND, ?, NOW(6))
                )
            )sql"
        },
        id,
        state_hash,
        provider,
        redirect_uri,
        static_cast<std::int64_t>(ttl.count())
    );

    return state;
}

std::optional<std::string> OAuthStateRepository::Consume(
    const std::string& state,
    const std::string& provider
) const {
    if (state.empty()) {
        return std::nullopt;
    }

    if (provider != "GOOGLE" && provider != "GITHUB") {
        return std::nullopt;
    }

    const std::string state_hash = HashState(state);

    // UPDATE dulu: hanya satu request yang bisa consume state
    // (consumed_at IS NULL + rows_affected == 0 berarti invalid/expired/pakai ulang).
    auto result = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                UPDATE oauth_states
                SET consumed_at = NOW(6)
                WHERE state_hash = ?
                  AND provider = ?
                  AND consumed_at IS NULL
                  AND expires_at > NOW(6)
            )sql"
        },
        state_hash,
        provider
    ).AsExecutionResult();

    if (result.rows_affected == 0) {
        return std::nullopt;
    }

    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                SELECT redirect_uri
                FROM oauth_states
                WHERE state_hash = ?
                LIMIT 1
            )sql"
        },
        state_hash
    ).AsOptionalSingleField<std::string>();
}

}  // namespace priemman::database
