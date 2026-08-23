#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <openssl/sha.h>
#include <userver/storages/mysql/cluster.hpp>

namespace priemman::database {

struct OtpRecord {
    std::string id;
    std::string otp_hash;
    // Kolom attempts bertipe INT UNSIGNED di DB; userver memvalidasi
    // signedness bind, jadi harus unsigned juga di sisi client.
    std::uint32_t attempts;
    // Ekspresi boolean `(locked_until IS NOT NULL AND ...)` dilaporkan
    // nullable oleh MySQL, jadi harus optional di sisi client.
    std::optional<std::int32_t> is_locked;
};

enum class OtpResult {
    kSuccess,
    kIpSuspended,
    kEmailSuspended,
    kInvalidCode,
    kNotFoundOrExpired
};

class OtpRepository {
public:
    explicit OtpRepository(
        std::shared_ptr<userver::storages::mysql::Cluster>* mysql_cluster
    );

    // Simpan challenge baru. Return false kalau masih cooldown.
    bool CreateChallenge(
        const std::string& email,
        const std::string& otp_hash,
        const std::string& ip_address,
        std::chrono::seconds ttl,
        std::chrono::seconds cooldown
    ) const;

    // Verifikasi OTP dan hanguskan (single-use). Return true kalau valid.
    OtpResult VerifyAndConsume(
        const std::string& email,
        const std::string& raw_otp,
        const std::string& ip_address
    ) const;

    // Hash SHA-256 untuk OTP code
    static std::string HashOtp(const std::string& otp_code);

private:
    std::shared_ptr<userver::storages::mysql::Cluster> _mysql_cluster{nullptr};
};

}  // namespace priemman::database
