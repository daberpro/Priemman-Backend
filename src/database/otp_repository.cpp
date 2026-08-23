#include "otp_repository.hpp"

#include <cstring>
#include <cstdint>
#include <iomanip>
#include <optional>
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
    const std::string& ip_address,
    std::chrono::seconds ttl,
    std::chrono::seconds cooldown
) const {

    // 1. Cek apakah Email atau IP sedang di-suspend
    const auto lock_status = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                SELECT 1 FROM otp_challenges
                WHERE (email = ? OR ip_address = ?)
                  AND locked_until > NOW(6)
                LIMIT 1
            )sql"
        },
        email, ip_address
    ).AsOptionalSingleField<std::int32_t>();

    if (lock_status.has_value()) {
        return false; // Sedang di-suspend, tolak pembuatan OTP
    }

    // 2. Cooldown: cek apakah ada pengiriman OTP dalam N detik terakhir
    const auto recent = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                SELECT 1
                FROM otp_challenges
                WHERE email = ?
                  AND last_sent_at > DATE_SUB(NOW(6), INTERVAL ? SECOND)
                LIMIT 1
            )sql"
        },
        email,
        static_cast<std::int64_t>(cooldown.count())
    ).AsOptionalSingleField<std::int32_t>();

    if (recent.has_value()) {
        return false;  // masih cooldown
    }

    // 3. Hanguskan challenge lama (consumed)
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

    // 4. Insert challenge baru (PERBAIKAN: ip_address ditambahkan)
    auto result = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                INSERT INTO otp_challenges (id, email, ip_address, otp_hash, expires_at, last_sent_at)
                VALUES (?, ?, ?, ?, DATE_ADD(NOW(6), INTERVAL ? SECOND), NOW(6))
            )sql"
        },
        userver::utils::generators::GenerateUuid(),
        email,
        ip_address,
        otp_hash,
        static_cast<std::int64_t>(ttl.count())
    ).AsExecutionResult();

    return result.rows_affected > 0;
}

OtpResult OtpRepository::VerifyAndConsume(
    const std::string& email,
    const std::string& raw_otp,
    const std::string& ip_address
) const {

    // SUM() menghasilkan NULL bila tidak ada baris yang cocok, jadi
    // harus di-bind sebagai optional agar tidak memicu InvariantError.
    const auto ip_fails = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            R"sql(
                SELECT SUM(attempts) as total_ip_fails
                FROM otp_challenges
                WHERE ip_address = ?
                  AND created_at > DATE_SUB(NOW(6), INTERVAL 15 MINUTE)
            )sql"
        },
        ip_address
    ).AsOptionalSingleField<std::optional<std::int64_t>>();

    // Jika total kegagalan dari IP ini sudah >= 5 kali dalam 15 menit terakhir, tolak!
    if (ip_fails.value_or(std::nullopt).value_or(0) >= 5) {
        return OtpResult::kIpSuspended;
    }

    auto challenge_opt = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                SELECT
                    id,
                    otp_hash,
                    attempts,
                    (locked_until IS NOT NULL AND locked_until > NOW(6)) AS is_locked
                FROM otp_challenges
                WHERE email = ? AND consumed_at IS NULL AND expires_at > NOW(6)
                ORDER BY created_at DESC LIMIT 1
            )sql"
        },
        email
    ).AsOptionalSingleRow<OtpRecord>();

    if (!challenge_opt.has_value()) {
        return OtpResult::kNotFoundOrExpired;
    }

    const auto& challenge = challenge_opt.value();
    if (challenge.is_locked.value_or(0) == 1) {
        return OtpResult::kEmailSuspended;
    }

    std::string input_hash = HashOtp(raw_otp);

    if (input_hash == challenge.otp_hash) {
        // SUKSES
        _mysql_cluster->Execute(
            userver::storages::mysql::ClusterHostType::kPrimary,
            "UPDATE otp_challenges SET consumed_at = NOW(6) WHERE id = ?",
            challenge.id
        );
        return OtpResult::kSuccess;
    }

    std::uint32_t current_attempts = challenge.attempts + 1;

    if (current_attempts < 3) {
        // Masih di bawah batas, cukup update nilai attempts
        _mysql_cluster->Execute(
            userver::storages::mysql::ClusterHostType::kPrimary,
            R"sql(
                UPDATE otp_challenges SET attempts = ? WHERE id = ?
            )sql",
            current_attempts, challenge.id
        );
        return OtpResult::kInvalidCode;
    } else {
        // Sudah mencapai batas (>=3), kunci email ini selama 5 menit
        _mysql_cluster->Execute(
            userver::storages::mysql::ClusterHostType::kPrimary,
            R"sql(
                UPDATE otp_challenges
                SET attempts = ?, locked_until = DATE_ADD(NOW(6), INTERVAL 5 MINUTE)
                WHERE id = ?
            )sql",
            current_attempts, challenge.id
        );
        return OtpResult::kEmailSuspended;
    }
}

}  // namespace priemman::database
