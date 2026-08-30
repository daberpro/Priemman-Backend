#pragma once
#include <vector>
#include <unordered_set>
#include <expected>
#include <string>
#include <proto/media.pb.h>
#include <src/database/media_repository.hpp>

namespace priemman::media {

inline void CollectPublicIds(
    const google::protobuf::RepeatedPtrField<priemman::v1::Media>& media,
    std::vector<std::string>* result
){
    std::unordered_set<std::string> seen;
    for(const auto& m : media){
        if(!m.public_id().empty() && seen.insert(m.public_id()).second){
            result->emplace_back(m.public_id());
        }
    }
}

inline std::expected<bool,std::string> ValidateMediaForCreate(
    const database::MediaRepository& media_repo,
    const std::vector<std::string>& public_ids,
    const std::string& user_id
) {
    for (const auto& pid : public_ids) {
        auto row = media_repo.FindByPublicId(pid);
        if (!row.has_value() || row->user_id != user_id) {
            return std::unexpected{"Media '" + pid + "' is invalid or does not belong to you"};
        }
        if (row->status != "orphan") {
            return std::unexpected{"Media '" + pid + "' is already used"};
        }
    }
    return true;
}


inline std::expected<bool, std::string> ValidateMediaForUpdate(
    const database::MediaRepository& media_repo,
    const std::vector<std::string>& public_ids,
    const std::string& user_id
) {
    for (const auto& pid : public_ids) {
        auto row = media_repo.FindByPublicId(pid);
        if (!row.has_value() || row->user_id != user_id) {
            return std::unexpected{"Media '" + pid + "' is invalid or does not belong to you"};
        }
    }
    return true;
}


inline void AttachMedia(
    const database::MediaRepository& media_repo,
    const std::vector<std::string>& public_ids,
    const std::string& user_id
) {
    for (const auto& pid : public_ids) {
        media_repo.MarkInUse(pid, user_id);
    }
}

inline void DetachMedia(
    const database::MediaRepository& media_repo,
    const std::vector<std::string>& public_ids,
    const std::string& user_id
){
    for (const auto& pid : public_ids) {
        media_repo.MakeOrphan(pid, user_id);
    }
}

}
