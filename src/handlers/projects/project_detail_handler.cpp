#include "project_detail_handler.hpp"

#include <proto/project.pb.h>
#include <userver/server/http/http_method.hpp>

#include <src/handlers/projects/project_helpers.hpp>
#include <src/handlers/projects/project_proto_convert.hpp>

#include <unordered_set>

namespace priemman::handlers::projects {

namespace {
using namespace userver::server::http;  // NOLINT

bool IsPubliclyVisible(const database::ProjectRow& row) {
    return row.status == "PUBLISHED" && row.visibility == "PUBLIC";
}
}  // namespace

std::string ProjectDetailHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&
) const {
    auto& res = request.GetHttpResponse();
    res.SetContentType("application/x-protobuf");

    const std::string id{request.GetPathArg("id")};
    if (id.empty()) {
        res.SetStatus(HttpStatus::kBadRequest);
        return ErrorResult("INVALID_ID", "Project id is required");
    }

    const auto viewer = TryAuth(request);
    const auto method = request.GetMethod();

    // ================= GET =================
    if (method == HttpMethod::kGet) {
        auto row = _projects.FindById(id);
        if (!row.has_value()) {
            res.SetStatus(HttpStatus::kNotFound);
            return ErrorResult("NOT_FOUND", "Project not found");
        }

        const bool is_owner = viewer.has_value() && *viewer == row->owner_id;
        if (!IsPubliclyVisible(*row) && !is_owner) {
            res.SetStatus(HttpStatus::kNotFound);
            return ErrorResult("NOT_FOUND", "Project not found");
        }

        if (IsPubliclyVisible(*row) && !is_owner) {
            _projects.IncrementViews(id);
        }

        priemman::v1::ProjectResponse response;
        *response.mutable_project() = mapper::ToProto(
            *row,
            _projects.ListStrings(id, "tools"),
            _projects.ListStrings(id, "disciplines"),
            _projects.ListStrings(id, "tags"),
            _projects.ListMedia(id),
            _projects.ListCollaborators(id)
        );
        return response.SerializeAsString();
    }

    // ================= PUT / DELETE (owner only) =================
    auto user_id = RequireAuth(request);
    if (!user_id.has_value()) {
        return ErrorResult("UNAUTHORIZED", "Missing or invalid session token");
    }

    if (method == HttpMethod::kPut) {
        priemman::v1::UpdateProjectRequest req;
        if (!req.ParseFromString(request.RequestBody()) || !req.has_input()) {
            res.SetStatus(HttpStatus::kBadRequest);
            return ErrorResult("INVALID_BODY", "Invalid request body");
        }

        const auto& input = req.input();

        auto existing = _projects.FindById(id);
        if (!existing.has_value() || existing->owner_id != *user_id) {
            res.SetStatus(HttpStatus::kNotFound);
            return ErrorResult("NOT_FOUND", "Project not found");
        }

        // Validasi media Cloudinary sebelum menyimpan apapun.
        // Media in_use yang memang sudah terpasang di project ini boleh
        // dikirim ulang; milik orang lain / tidak dikenal ditolak tegas.
        const auto public_ids = helpers::CollectPublicIds(input.media());
        if (const auto err = helpers::ValidateMediaForUpdate(
                _media, _projects, id, public_ids, *user_id);
            !err.empty()) {
            res.SetStatus(HttpStatus::kBadRequest);
            return ErrorResult("INVALID_MEDIA", err);
        }

        // Simpan media lama sebelum update
        auto old_media = _projects.ListMedia(id);

        database::ProjectRow row;
        row.id = id;
        row.owner_id = *user_id;
        row.title = input.title();
        row.slug = existing->slug;
        row.description = input.description();
        row.visibility = mapper::VisibilityToString(input.visibility());
        row.status = mapper::StatusToString(input.status());

        if (!_projects.Update(row)) {
            res.SetStatus(HttpStatus::kNotFound);
            return ErrorResult("NOT_FOUND", "Project not found");
        }

        helpers::SaveChildren(_projects, id, input);
        helpers::AttachMedia(_media, public_ids, *user_id);

        // Ambil media baru dan kembalikan yang tidak terpakai ke orphan.
        // Penghapusan dari Cloudinary diserahkan ke sweeper (grace period).
        auto new_media = _projects.ListMedia(id);

        std::unordered_set<std::string> new_public_ids;
        for (const auto& m : new_media) {
            if (!m.cloudinary_public_id.empty()) {
                new_public_ids.insert(m.cloudinary_public_id);
            }
        }

        for (const auto& old : old_media) {
            if (!old.cloudinary_public_id.empty() &&
                new_public_ids.find(old.cloudinary_public_id) == new_public_ids.end()) {
                _media.DetachIfUnreferenced(old.cloudinary_public_id);
            }
        }

        auto updated = _projects.FindById(id);
        priemman::v1::ProjectResponse response;
        if (updated.has_value()) {
            *response.mutable_project() = mapper::ToProto(
                *updated,
                _projects.ListStrings(id, "tools"),
                _projects.ListStrings(id, "disciplines"),
                _projects.ListStrings(id, "tags"),
                _projects.ListMedia(id),
                _projects.ListCollaborators(id)
            );
        }
        return response.SerializeAsString();
    }

    if (method == HttpMethod::kDelete) {
        // Ambil media project sebelum dihapus
        auto media_list = _projects.ListMedia(id);

        if (!_projects.Delete(id, *user_id)) {
            res.SetStatus(HttpStatus::kNotFound);
            return ErrorResult("NOT_FOUND", "Project not found");
        }

        // Kembalikan semua media ke orphan — sweeper yang akan menghapusnya
        // dari Cloudinary setelah grace period.
        for (const auto& m : media_list) {
            if (!m.cloudinary_public_id.empty()) {
                _media.DetachIfUnreferenced(m.cloudinary_public_id);
            }
        }

        priemman::v1::DeleteResponse response;
        response.set_success(true);
        return response.SerializeAsString();
    }

    res.SetStatus(HttpStatus::kMethodNotAllowed);
    return ErrorResult("METHOD_NOT_ALLOWED", "Unsupported method");
}

}  // namespace priemman::handlers::projects
