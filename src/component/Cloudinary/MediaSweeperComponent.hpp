#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include <userver/components/loggable_component_base.hpp>
#include <userver/engine/task/task_with_result.hpp>
#include <userver/storages/mysql/cluster.hpp>
#include <userver/yaml_config/yaml_config.hpp>

#include <src/component/Cloudinary/CloudinaryClientComponent.hpp>
#include <src/database/media_repository.hpp>

namespace priemman::cloudinary {

// Komponen terjadwal yang membersihkan media orphan dari Cloudinary:
// aset yang diupload tapi tidak pernah di-attach ke project, atau yang
// dikembalikan ke status orphan setelah project dihapus/diupdate.
class MediaSweeperComponent final : public userver::components::LoggableComponentBase {
public:
    static constexpr std::string_view kName{"media-sweeper"};

    MediaSweeperComponent(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );
    ~MediaSweeperComponent() override;

    static userver::yaml_config::Schema GetStaticConfigSchema();

private:
    void SweepLoop();
    void SweepOnce();

    std::shared_ptr<userver::storages::mysql::Cluster> mysql_cluster_;
    database::MediaRepository media_repo_;
    const Client& cloudinary_client_;
    std::chrono::seconds interval_;
    std::int64_t ttl_seconds_;
    std::int64_t batch_limit_;
    userver::engine::TaskWithResult<void> sweep_task_;
};

}  // namespace priemman::cloudinary
