#include "otp_repository.hpp"

#include <cstring>
#include <iomanip>
#include <sstream>

#include <userver/storages/mysql/cluster_host_type.hpp>
#include <userver/storages/mysql/query.hpp>
#include <userver/utils/uuid4.hpp>

namespace priemman::database {

OtpRepository::OtpRepository(
    std::shared_ptr<userver::storages::mysql::Cluster>* mysql_cluster
)
    : _mysql_cluster(*mysql_cluster) {
}

std::string OtpRepository::HashOtp(const std::string& otp_code) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(otp_code.c_str()),
           otp_code.size(), hash);

    std::ostringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

bool OtpRepository::CreateChallenge(
    const std::string& email,
    const std::string& otp_hash,
    std::chrono::seconds ttl,
    std::chrono::seconds cooldown
) const {
    // 1. Cooldown: cek apakah ada pengiriman OTP dalam N detik terakhir
    const auto recent = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                SELECT COUNT(*)
                FROM otp_challenges
                WHERE email = ?
                  AND last_sent_at > DATE_SUB(NOW(6), INTERVAL ? SECOND)
            )sql"
        },
        email,
        static_cast<std::int64_t>(cooldown.count())
    ).AsOptionalSingleField<std::int64_t>();

    if (recent.value_or(0) > 0) {
        return false;  // masih cooldown
    }

    // 2. Hanguskan challenge lama (consumed)
    _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                UPDATE otp_challenges
                SET consumed_at = NOW(6)
                WHERE email = ?
                  AND consumed_at IS NULL
            )sql"
        },
        email
    );

    // 3. Insert challenge baru
    auto result = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                INSERT INTO otp_challenges (id, email, otp_hash, expires_at, last_sent_at)
                VALUES (?, ?, ?, DATE_ADD(NOW(6), INTERVAL ? SECOND), NOW(6))
            )sql"
        },
        userver::utils::generators::GenerateUuid(),
        email,
        otp_hash,
        static_cast<std::int64_t>(ttl.count())
    ).AsExecutionResult();

    return result.rows_affected > 0;
}

bool OtpRepository::VerifyAndConsume(
    const std::string& email,
    const std::string& otp_hash
) const {
    // Cari challenge yang valid (belum expired, belum consumed)
    auto result = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                UPDATE otp_challenges
                SET consumed_at = NOW(6)
                WHERE email = ?
                  AND otp_hash = ?
                  AND consumed_at IS NULL
                  AND expires_at > NOW(6)
                LIMIT 1
            )sql"
        },
        email,
        otp_hash
    ).AsExecutionResult();

    return result.rows_affected > 0;
}

}  // namespace priemman::database
