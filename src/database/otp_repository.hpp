#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <openssl/sha.h>
#include <userver/storages/mysql/cluster.hpp>

namespace priemman::database {

class OtpRepository {
public:
    explicit OtpRepository(
        std::shared_ptr<userver::storages::mysql::Cluster>* mysql_cluster
    );

    // Simpan challenge baru. Return false kalau masih cooldown.
    bool CreateChallenge(
        const std::string& email,
        const std::string& otp_hash,
        std::chrono::seconds ttl,
        std::chrono::seconds cooldown
    ) const;

    // Verifikasi OTP dan hanguskan (single-use). Return true kalau valid.
    bool VerifyAndConsume(
        const std::string& email,
        const std::string& otp_hash
    ) const;

    // Hash SHA-256 untuk OTP code
    static std::string HashOtp(const std::string& otp_code);

private:
    std::shared_ptr<userver::storages::mysql::Cluster> _mysql_cluster{nullptr};
};

}  // namespace priemman::database
