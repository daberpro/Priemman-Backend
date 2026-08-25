#include "upload_media_handler.hpp"

#include <map>

#include <userver/server/handlers/exceptions.hpp>
#include <userver/utils/uuid4.hpp>

#include <userver/http/common_headers.hpp>
#include <userver/http/content_type.hpp>
#include <userver/logging/log.hpp>

#include <proto/media.pb.h>
#include <proto/project.pb.h>

namespace priemman::handlers::media {

namespace {

constexpr std::size_t kMaxFileSize = 10 * 1024 * 1024; // 10MB per file
constexpr std::size_t kMaxFiles = 10;                  // maksimal file per request

// Optimasi delivery: sisipkan transformasi f_auto,q_auto supaya CDN
// Cloudinary menyajikan AVIF (Chrome/Firefox) / WebP (browser lain)
// sesuai kemampuan browser, dengan fallback ke format asli.
// Aset asli tetap utuh; hanya URL delivery-nya yang berubah.
// Contoh: .../image/upload/v123/x.png -> .../image/upload/f_auto,q_auto/v123/x.png
std::string InsertDeliveryTransform(std::string media_url) {
    constexpr std::string_view kUploadMarker = "/upload/";
    const auto marker_pos = media_url.find(kUploadMarker);
    if (media_url.find("res.cloudinary.com") != std::string::npos &&
        marker_pos != std::string::npos) {
        media_url.insert(marker_pos + kUploadMarker.size(), "f_auto,q_auto/");
    }
    return media_url;
}

priemman::v1::MediaType MediaTypeFromCloudinary(
    const std::string& resource_type,
    const std::string& content_type
) {
    if (resource_type == "image") return priemman::v1::MEDIA_TYPE_IMAGE;
    if (resource_type == "video") return priemman::v1::MEDIA_TYPE_VIDEO;
    if (content_type.find("image/") == 0) return priemman::v1::MEDIA_TYPE_IMAGE;
    if (content_type.find("video/") == 0) return priemman::v1::MEDIA_TYPE_VIDEO;
    return priemman::v1::MEDIA_TYPE_UNSPECIFIED;
}

} // namespace

UploadMediaHandler::UploadMediaHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : AuthenticatedHandlerBase(config, context),
      cloudinary_client_(
          context.FindComponent<cloudinary::CloudinaryComponent>().GetClient()
      ) {}

std::string UploadMediaHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext& /*context*/
) const {
    auto& res = request.GetHttpResponse();

    auto return_error = [&](userver::server::http::HttpStatus status,
                            const std::string& code,
                            const std::string& message) -> std::string {
        res.SetStatus(status);
        res.SetContentType("application/protobuf");
        return ErrorResult(code, message);
    };

    // 1. Auth
    const auto user_id = RequireAuth(request);
    if (!user_id.has_value()) {
        res.SetContentType("application/protobuf");
        return ErrorResult("UNAUTHORIZED", "User is not authenticated");
    }

    // 2. Validasi Content-Type
    const auto content_type_header = request.GetHeader(userver::http::headers::kContentType);
    const auto content_type = userver::http::ContentType(content_type_header);

    if (content_type.MediaType() != "multipart/form-data") {
        return return_error(userver::server::http::HttpStatus::kBadRequest,
                            "INVALID_CONTENT_TYPE",
                            "Expected 'multipart/form-data', got: " + content_type_header);
    }

    // 3. Ambil semua file dari form data (field "file" boleh dikirim lebih dari satu)
    if (!request.HasFormDataArg("file")) {
        return return_error(userver::server::http::HttpStatus::kBadRequest,
                            "MISSING_FILE", "Missing 'file' in form data");
    }

    const auto& files = request.GetFormDataArgVector("file");
    if (files.size() > kMaxFiles) {
        return return_error(userver::server::http::HttpStatus::kBadRequest,
                            "MAX_FILES_EXCEEDED",
                            fmt::format("Too many files: {} (max: {})",
                                        files.size(), kMaxFiles));
    }

    // 4. Validasi awal semua file sebelum upload
    for (const auto& file_arg : files) {
        const std::string filename = file_arg.filename.value_or("unknown");
        if (file_arg.value.empty()) {
            return return_error(userver::server::http::HttpStatus::kBadRequest,
                                "EMPTY_FILE", "Uploaded file is empty: " + filename);
        }
        if (file_arg.value.size() > kMaxFileSize) {
            return return_error(userver::server::http::HttpStatus::kBadRequest,
                                "FILE_TOO_LARGE",
                                fmt::format("File too large: {} ({} bytes, max: {} bytes)",
                                            filename, file_arg.value.size(), kMaxFileSize));
        }
    }

    priemman::v1::UploadMediaBatchResponse batch_res;

    for (const auto& file_arg : files) {
        const std::string filename = file_arg.filename.value_or("upload.bin");

        LOG_INFO() << "Uploading file: " << filename
                   << " size: " << file_arg.value.size()
                   << " bytes"
                   << " content-type: " << file_arg.content_type.value_or("unknown");

        // PENTING: copy dari string_view dengan panjang yang benar.
        // Jangan pakai .data() — string_view tidak null-terminated, konstruktor
        // std::string(const char*) akan membaca lewat batas view dan menelan
        // "\r\n\r\n" + bytes file, merusak Content-Type part multipart.
        std::string content_type_str{file_arg.content_type.value_or("application/octet-stream")};

        // Tentukan resource_type dari content-type
        std::string resource_type = "auto";
        if (content_type_str.find("image/") == 0) {
            resource_type = "image";
        } else if (content_type_str.find("video/") == 0) {
            resource_type = "video";
        }

        std::map<std::string, std::string> additional_params = {
            {"folder", "projects/" + *user_id},
            {"unique_filename", "true"},
        };

        // Catatan: parameter expires_at sengaja TIDAK dikirim — di akun paket
        // Free tidak ikut dihitung dalam signature sehingga membuat upload 401,
        // dan di explicit API diabaikan. Pembersihan orphan cukup lewat sweeper DB.

        // 5. Upload ke Cloudinary
        std::string response_body;
        try {
            LOG_INFO() << "Uploading to Cloudinary: folder=projects/" << *user_id
                       << ", resource_type=" << resource_type
                       << ", file=" << filename;

            response_body = cloudinary_client_.UploadFile(
                std::string(file_arg.value),
                filename,
                content_type_str,
                additional_params,
                resource_type
            );
        } catch (const std::exception& e) {
            LOG_ERROR() << "Cloudinary upload failed: " << e.what();
            return return_error(
                userver::server::http::HttpStatus::kInternalServerError,
                "CLOUDINARY_ERROR",
                std::string("Upload failed: ") + e.what()
            );
        }

        // 6. Parse response JSON dari Cloudinary
        auto json = userver::formats::json::FromString(response_body);

        LOG_INFO() << "Cloudinary upload success: public_id="
                   << json["public_id"].As<std::string>("");

        // 7. Map ke protobuf UploadMediaResponse
        auto* item = batch_res.add_items();

        std::string media_url = json["secure_url"].As<std::string>(json["url"].As<std::string>(""));
        item->set_url(InsertDeliveryTransform(std::move(media_url)));

        // Public ID (wajib untuk operasi hapus nanti)
        item->set_public_id(json["public_id"].As<std::string>(""));

        // Resource type dari Cloudinary response
        const auto cloudinary_resource_type = json["resource_type"].As<std::string>("auto");
        item->set_resource_type(cloudinary_resource_type);

        // 8. Catat aset sebagai orphan — kalau tidak pernah di-attach ke project,
        // sweeper akan menghapusnya dari Cloudinary setelah TTL.
        const std::string public_id = item->public_id();
        if (!public_id.empty()) {
            try {
                _media.InsertOrphan(public_id, *user_id, cloudinary_resource_type);
            } catch (const std::exception& e) {
                // Aset sudah ada di Cloudinary tapi tidak tercatat; sweeper tidak
                // akan mengenalinya, namun upload gagal di sisi client.
                LOG_ERROR() << "Failed to track media upload " << public_id
                            << ": " << e.what();
                return return_error(
                    userver::server::http::HttpStatus::kInternalServerError,
                    "DB_ERROR", "Failed to record uploaded media"
                );
            }
        }

        item->set_type(MediaTypeFromCloudinary(cloudinary_resource_type, content_type_str));

        // Generate UUID untuk media id
        item->mutable_id()->set_value(userver::utils::generators::GenerateUuid());
    }

    res.SetContentType("application/protobuf");
    return batch_res.SerializeAsString();
}

} // namespace priemman::handlers::media
