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

    // 3. Ambil file dari form data
    if (!request.HasFormDataArg("file")) {
        return return_error(userver::server::http::HttpStatus::kBadRequest,
                            "MISSING_FILE", "Missing 'file' in form data");
    }

    const auto& file_arg = request.GetFormDataArg("file");
    if (file_arg.value.empty()) {
        return return_error(userver::server::http::HttpStatus::kBadRequest,
                            "EMPTY_FILE", "Uploaded file is empty");
    }

    // 4. Log info file
    LOG_INFO() << "Uploading file: "
               << file_arg.filename.value_or("unknown")
               << " size: " << file_arg.value.size()
               << " bytes"
               << " content-type: " << file_arg.content_type.value_or("unknown");

    // 5. Cek ukuran file (max 10MB)
    const size_t MAX_FILE_SIZE = 10 * 1024 * 1024; // 10MB
    if (file_arg.value.size() > MAX_FILE_SIZE) {
        return return_error(userver::server::http::HttpStatus::kBadRequest,
                            "FILE_TOO_LARGE",
                            fmt::format("File too large: {} bytes (max: {} bytes)",
                                       file_arg.value.size(), MAX_FILE_SIZE));
    }

    // 6. Siapkan parameter untuk Cloudinary
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

    // 7. Upload ke Cloudinary
    std::string response_body;
    try {
        LOG_INFO() << "Uploading to Cloudinary: folder=projects/" << *user_id
                   << ", resource_type=" << resource_type;

        response_body = cloudinary_client_.UploadFile(
            std::string(file_arg.value),
            file_arg.filename.value_or("upload.bin"),
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

    // 8. Parse response JSON dari Cloudinary
    auto json = userver::formats::json::FromString(response_body);

    LOG_INFO() << "Cloudinary upload success: public_id="
               << json["public_id"].As<std::string>("");

    // 9. Map ke protobuf UploadMediaResponse
    priemman::v1::UploadMediaResponse proto_res;

    // URL
    std::string media_url = json["secure_url"].As<std::string>(json["url"].As<std::string>(""));

    // Optimasi delivery: sisipkan transformasi f_auto,q_auto supaya CDN
    // Cloudinary menyajikan AVIF (Chrome/Firefox) / WebP (browser lain)
    // sesuai kemampuan browser, dengan fallback ke format asli.
    // Aset asli tetap utuh; hanya URL delivery-nya yang berubah.
    // Contoh: .../image/upload/v123/x.png -> .../image/upload/f_auto,q_auto/v123/x.png
    constexpr std::string_view kUploadMarker = "/upload/";
    const auto marker_pos = media_url.find(kUploadMarker);
    if (media_url.find("res.cloudinary.com") != std::string::npos &&
        marker_pos != std::string::npos) {
        media_url.insert(marker_pos + kUploadMarker.size(), "f_auto,q_auto/");
    }
    proto_res.set_url(media_url);

    // Public ID (wajib untuk operasi hapus nanti)
    proto_res.set_public_id(json["public_id"].As<std::string>(""));

    // Resource type dari Cloudinary response
    const auto cloudinary_resource_type = json["resource_type"].As<std::string>("auto");
    proto_res.set_resource_type(cloudinary_resource_type);

    // 10. Catat aset sebagai orphan — kalau tidak pernah di-attach ke project,
    // sweeper akan menghapusnya dari Cloudinary setelah TTL.
    const std::string public_id = proto_res.public_id();
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

    // Tentukan MediaType
    if (cloudinary_resource_type == "image") {
        proto_res.set_type(priemman::v1::MEDIA_TYPE_IMAGE);
    } else if (cloudinary_resource_type == "video") {
        proto_res.set_type(priemman::v1::MEDIA_TYPE_VIDEO);
    } else {
        // Fallback ke content-type
        if (content_type_str.find("image/") == 0) {
            proto_res.set_type(priemman::v1::MEDIA_TYPE_IMAGE);
        } else if (content_type_str.find("video/") == 0) {
            proto_res.set_type(priemman::v1::MEDIA_TYPE_VIDEO);
        } else {
            proto_res.set_type(priemman::v1::MEDIA_TYPE_UNSPECIFIED);
        }
    }

    // Generate UUID untuk media id
    proto_res.mutable_id()->set_value(userver::utils::generators::GenerateUuid());

    res.SetContentType("application/protobuf");
    return proto_res.SerializeAsString();
}

} // namespace priemman::handlers::media
