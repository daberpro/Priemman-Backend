#include "MediaSweeperComponent.hpp"

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/engine/sleep.hpp>
#include <userver/engine/task/cancel.hpp>
#include <userver/storages/mysql/component.hpp>
#include <userver/utils/async.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace priemman::cloudinary {

MediaSweeperComponent::MediaSweeperComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : LoggableComponentBase(config, context),
      mysql_cluster_(
          context.FindComponent<userver::storages::mysql::Component>("database").GetCluster()
      ),
      media_repo_(&mysql_cluster_),
      cloudinary_client_(
          context.FindComponent<CloudinaryComponent>().GetClient()
      ),
      interval_(config["interval-seconds"].As<std::int64_t>(3600)),
      ttl_seconds_(config["ttl-seconds"].As<std::int64_t>(database::kMediaOrphanTtlSeconds)),
      batch_limit_(config["batch-limit"].As<std::int64_t>(100)),
      sweep_task_(
          userver::utils::Async(
              context.GetTaskProcessor(
                  config["task-processor"].As<std::string>("main-task-processor")
              ),
              "media-sweeper-loop",
              [this] { SweepLoop(); }
          )
      ) {
    LOG_INFO() << "Media sweeper started: interval=" << interval_.count()
               << "s, ttl=" << ttl_seconds_ << "s, batch=" << batch_limit_;
}

MediaSweeperComponent::~MediaSweeperComponent() {
    sweep_task_.RequestCancel();
    try {
        sweep_task_.Wait();
    } catch (const std::exception& e) {
        LOG_ERROR() << "Media sweeper stopped with error: " << e.what();
    }
}

void MediaSweeperComponent::SweepLoop() {
    while (!userver::engine::current_task::ShouldCancel()) {
        try {
            SweepOnce();
        } catch (const std::exception& e) {
            LOG_ERROR() << "Media sweep failed: " << e.what();
        }
        userver::engine::InterruptibleSleepFor(interval_);
    }
}

void MediaSweeperComponent::SweepOnce() {
    const auto orphans = media_repo_.ListExpiredOrphans(ttl_seconds_, batch_limit_);
    if (orphans.empty()) return;

    LOG_INFO() << "Media sweeper: found " << orphans.size() << " expired orphan(s)";

    for (const auto& orphan : orphans) {
        if (userver::engine::current_task::ShouldCancel()) break;

        // destroy endpoint hanya ada untuk image/video
        if (orphan.resource_type != "image" && orphan.resource_type != "video") {
            LOG_WARNING() << "Media sweeper: unsupported resource_type '"
                          << orphan.resource_type << "' for public_id="
                          << orphan.public_id;
            continue;
        }

        bool deleted = false;
        try {
            deleted = cloudinary_client_.Delete(orphan.public_id, orphan.resource_type);
        } catch (const std::exception& e) {
            LOG_WARNING() << "Media sweeper: failed to delete public_id="
                          << orphan.public_id << ": " << e.what();
        }

        // Baris DB baru dihapus setelah Cloudinary mengonfirmasi —
        // kalau gagal, aset dicoba lagi di sweep berikutnya (idempotent).
        if (deleted) {
            media_repo_.DeleteByPublicId(orphan.public_id);
        }
    }
}

userver::yaml_config::Schema MediaSweeperComponent::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<userver::components::LoggableComponentBase>(R"(
type: object
description: Periodic cleaner for orphaned Cloudinary media
additionalProperties: false
properties:
    task-processor:
        type: string
        description: Task processor to run the sweep loop on
        defaultDescription: main-task-processor
    interval-seconds:
        type: integer
        description: How often to run the sweep
        defaultDescription: 3600
    ttl-seconds:
        type: integer
        description: Orphan age (seconds) after which media is deleted
        defaultDescription: 86400
    batch-limit:
        type: integer
        description: Max orphans processed per sweep
        defaultDescription: 100
)");
}

}  // namespace priemman::cloudinary
