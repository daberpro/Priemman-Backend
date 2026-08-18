#pragma once

#include <cctype>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include <proto/project.pb.h>
#include <userver/utils/uuid4.hpp>

#include <src/database/media_repository.hpp>
#include <src/database/project_repository.hpp>

namespace priemman::handlers::projects::helpers {

inline std::string Slugify(const std::string& s) {
    std::string out;
    bool dashed = false;
    for (const char c : s) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            dashed = false;
        } else if (!dashed && !out.empty()) {
            out += '-';
            dashed = true;
        }
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out.empty() ? "project" : out;
}

inline std::vector<database::ProjectMediaRow> BuildMediaRows(
    const google::protobuf::RepeatedPtrField<priemman::v1::Media>& media
) {
    std::vector<database::ProjectMediaRow> rows;
    for (const auto& m : media) {
        database::ProjectMediaRow row;
        row.id = m.id().value().empty()
            ? userver::utils::generators::GenerateUuid()
            : m.id().value();
        row.url = m.url();
        row.media_type = m.type() == priemman::v1::MEDIA_TYPE_VIDEO ? "VIDEO" : "IMAGE";
        row.sort_order = m.order();
        row.cloudinary_public_id = m.public_id(); // <-- ambil
        rows.push_back(std::move(row));
    }
    return rows;
}

inline std::vector<database::ProjectCollaboratorRow> BuildCollabRows(
    const google::protobuf::RepeatedPtrField<priemman::v1::Collaborator>& collabs
) {
    std::vector<database::ProjectCollaboratorRow> rows;
    for (const auto& c : collabs) {
        if (!c.user_id().value().empty()) {
            rows.push_back({c.user_id().value(), c.role()});
        }
    }
    return rows;
}

// Kumpulkan public_id unik dan tidak kosong dari media di input
inline std::vector<std::string> CollectPublicIds(
    const google::protobuf::RepeatedPtrField<priemman::v1::Media>& media
) {
    std::unordered_set<std::string> seen;
    std::vector<std::string> out;
    for (const auto& m : media) {
        if (!m.public_id().empty() && seen.insert(m.public_id()).second) {
            out.push_back(m.public_id());
        }
    }
    return out;
}

// CREATE: media harus tercatat, milik user ini, dan masih orphan.
// Return pesan error, kosong kalau valid.
inline std::string ValidateMediaForCreate(
    const database::MediaRepository& media_repo,
    const std::vector<std::string>& public_ids,
    const std::string& user_id
) {
    for (const auto& pid : public_ids) {
        auto row = media_repo.FindByPublicId(pid);
        if (!row.has_value() || row->user_id != user_id) {
            return "Media '" + pid + "' is invalid or does not belong to you";
        }
        if (row->status != "orphan") {
            return "Media '" + pid + "' is already used by another project";
        }
    }
    return {};
}

// UPDATE: sama seperti create, tetapi media in_use yang memang sudah
// terpasang di project ini boleh dikirim ulang.
inline std::string ValidateMediaForUpdate(
    const database::MediaRepository& media_repo,
    const database::ProjectRepository& projects,
    const std::string& project_id,
    const std::vector<std::string>& public_ids,
    const std::string& user_id
) {
    for (const auto& pid : public_ids) {
        auto row = media_repo.FindByPublicId(pid);
        if (!row.has_value() || row->user_id != user_id) {
            return "Media '" + pid + "' is invalid or does not belong to you";
        }
        if (row->status == "orphan") continue;
        if (row->status == "in_use" &&
            projects.IsMediaReferenced(project_id, pid)) {
            continue;
        }
        return "Media '" + pid + "' is already used by another project";
    }
    return {};
}

// Flip orphan -> in_use untuk semua media yang baru disimpan.
inline void AttachMedia(
    const database::MediaRepository& media_repo,
    const std::vector<std::string>& public_ids,
    const std::string& user_id
) {
    for (const auto& pid : public_ids) {
        media_repo.MarkInUse(pid, user_id);
    }
}

inline void SaveChildren(
    const database::ProjectRepository& repo,
    const std::string& project_id,
    const priemman::v1::ProjectInput& input
) {
    repo.ReplaceStrings(project_id, "tools",
        {input.tools().begin(), input.tools().end()});
    repo.ReplaceStrings(project_id, "disciplines",
        {input.disciplines().begin(), input.disciplines().end()});
    repo.ReplaceStrings(project_id, "tags",
        {input.tags().begin(), input.tags().end()});

    const auto media = BuildMediaRows(input.media());
    repo.ReplaceMedia(project_id, media);

    std::optional<std::string> cover;
    if (!input.cover_media_id().empty()) {
        cover = input.cover_media_id();
    } else if (!media.empty()) {
        cover = media.front().id;
    }
    repo.SetCover(project_id, cover);

    repo.ReplaceCollaborators(project_id, BuildCollabRows(input.collaborators()));
}

}  // namespace priemman::handlers::projects::helpers
