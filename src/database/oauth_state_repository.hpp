#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>

#include <userver/storages/mysql/cluster.hpp>

namespace priemman::database {

class OAuthStateRepository {
public:
    explicit OAuthStateRepository(
        std::shared_ptr<userver::storages::mysql::Cluster>* mysql_cluster
    );

    // Membuat state CSRF untuk OAuth.
    // Return value = state plaintext untuk dikirim ke browser/provider.
    // Yang disimpan di DB hanya hash-nya.
    std::string Create(
        const std::string& provider,
        const std::string& redirect_uri,
        std::chrono::seconds ttl
    ) const;

    // Validasi + consume state (single-use).
    // Return redirect_uri kalau state valid, nullopt kalau tidak.
    std::optional<std::string> Consume(
        const std::string& state,
        const std::string& provider
    ) const;

private:
    std::shared_ptr<userver::storages::mysql::Cluster> _mysql_cluster{nullptr};
};

}  // namespace priemman::database
