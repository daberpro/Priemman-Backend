#include "upload_media_handler.hpp"

#include <fmt/format.h>

#include <map>
#include <string>
#include <string_view>

#include <userver/formats/json.hpp>
#include <userver/http/common_headers.hpp>
#include <userver/http/content_type.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/exceptions.hpp>
#include <userver/utils/uuid4.hpp>

#include <proto/media.pb.h>
#include <proto/project.pb.h>

namespace priemman::handlers::media {

namespace {

constexpr std::size_t kMaxFileSize = 10 * 1024 * 1024;  // 10 MB
constexpr std::size_t kMaxFiles = 10;

priemman::v1::MediaType MediaTypeFromCloudinary(
    const std::string& resource_type,
    const std::string& content_type
) {
    if (resource_type == "image") {
        return priemman::v1::MEDIA_TYPE_IMAGE;
    }

    if (resource_type == "video") {
        return priemman::v1::MEDIA_TYPE_VIDEO;
    }

    if (content_type.find("image/") == 0) {
        return priemman::v1::MEDIA_TYPE_IMAGE;
    }

    if (content_type.find("video/") == 0) {
        return priemman::v1::MEDIA_TYPE_VIDEO;
    }

    return priemman::v1::MEDIA_TYPE_UNSPECIFIED;
}

}  // namespace

UploadMediaHandler::UploadMediaHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : AuthenticatedHandlerBase(config, context),
      cloudinary_client_(
          context.FindComponent<
              cloudinary::CloudinaryComponent
          >().GetClient()
      )
      // , avif_converter_(
      //       context.FindComponent<
      //           daberdev::components::AvifConvertComponent
      //       >()
      //   )
{}

std::string UploadMediaHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext& /*context*/
) const {
    auto& res = request.GetHttpResponse();

    auto return_error =
        [&](userver::server::http::HttpStatus status,
            const std::string& code,
            const std::string& message) -> std::string {
        res.SetStatus(status);
        res.SetContentType("application/protobuf");

        return ErrorResult(code, message);
    };

    const auto user_id = RequireAuth(request);

    if (!user_id.has_value()) {
        res.SetContentType("application/protobuf");

        return ErrorResult(
            "UNAUTHORIZED",
            "User is not authenticated"
        );
    }

    const auto content_type_header =
        request.GetHeader(userver::http::headers::kContentType);

    const auto content_type =
        userver::http::ContentType(content_type_header);

    if (content_type.MediaType() != "multipart/form-data") {
        return return_error(
            userver::server::http::HttpStatus::kBadRequest,
            "INVALID_CONTENT_TYPE",
            "Expected 'multipart/form-data', got: " +
                content_type_header
        );
    }

    if (!request.HasFormDataArg("file")) {
        return return_error(
            userver::server::http::HttpStatus::kBadRequest,
            "MISSING_FILE",
            "Missing 'file' in form data"
        );
    }

    const auto& files =
        request.GetFormDataArgVector("file");

    if (files.size() > kMaxFiles) {
        return return_error(
            userver::server::http::HttpStatus::kBadRequest,
            "MAX_FILES_EXCEEDED",
            fmt::format(
                "Too many files: {} (max: {})",
                files.size(),
                kMaxFiles
            )
        );
    }

    for (const auto& file_arg : files) {
        const std::string filename =
            file_arg.filename.value_or("unknown");

        if (file_arg.value.empty()) {
            return return_error(
                userver::server::http::HttpStatus::kBadRequest,
                "EMPTY_FILE",
                "Uploaded file is empty: " + filename
            );
        }

        if (file_arg.value.size() > kMaxFileSize) {
            return return_error(
                userver::server::http::HttpStatus::kBadRequest,
                "FILE_TOO_LARGE",
                fmt::format(
                    "File too large: {} ({} bytes, max: {} bytes)",
                    filename,
                    file_arg.value.size(),
                    kMaxFileSize
                )
            );
        }
    }

    priemman::v1::UploadMediaBatchResponse batch_res;

    for (const auto& file_arg : files) {
        const std::string original_filename =
            file_arg.filename.value_or("upload.bin");

        const std::string original_content_type{
            file_arg.content_type.value_or(
                "application/octet-stream"
            )
        };

        LOG_INFO()
            << "Processing file: "
            << original_filename
            << " size: "
            << file_arg.value.size()
            << " bytes"
            << " content-type: "
            << original_content_type;

        std::string upload_buffer{
            file_arg.value
        };

        std::string upload_filename{
            original_filename
        };

        std::string upload_content_type{
            original_content_type
        };

        std::string resource_type = "auto";

        /*
         * AVIF CONVERSION DISABLED
         *
         * Originally, every non-AVIF image was converted to AVIF
         * before being uploaded to Cloudinary.
         *
         * This conversion is currently disabled because AVIF encoding
         * is CPU-intensive and the server only has 2 CPU cores.
         *
         * The original image buffer, filename, and content type are
         * now passed directly to Cloudinary.
         *
         * The original implementation is kept below in comments so
         * it can be enabled again later if needed.
         */

        /*
        // AVIF tidak perlu dikonversi lagi.
        const bool is_image =
            original_content_type.find("image/") == 0;

        const bool is_avif =
            original_content_type == "image/avif";

        if (is_image && !is_avif) {
            LOG_INFO()
                << "Image detected, converting to AVIF: "
                << original_filename;

            auto converted =
                avif_converter_.ConvertBufferAsync(
                    upload_buffer
                );

            if (!converted.has_value()) {
                LOG_ERROR()
                    << "AVIF conversion failed for "
                    << original_filename
                    << ": "
                    << converted.error();

                return return_error(
                    userver::server::http::HttpStatus::kBadRequest,
                    "AVIF_CONVERSION_FAILED",
                    "Failed to convert image to AVIF: " +
                        converted.error()
                );
            }

            upload_buffer = std::move(*converted);

            const auto extension_pos =
                upload_filename.find_last_of('.');

            if (extension_pos != std::string::npos) {
                upload_filename.replace(
                    extension_pos,
                    std::string::npos,
                    ".avif"
                );
            } else {
                upload_filename += ".avif";
            }

            upload_content_type = "image/avif";
            resource_type = "image";

            LOG_INFO()
                << "AVIF conversion success: "
                << original_filename
                << " -> "
                << upload_filename
                << " size: "
                << upload_buffer.size()
                << " bytes";
        }
        else if (is_avif) {
            // AVIF sudah dalam format target, jadi upload langsung.
            resource_type = "image";

            LOG_INFO()
                << "AVIF detected, skipping conversion: "
                << original_filename
                << " size: "
                << upload_buffer.size()
                << " bytes";
        }
        else if (original_content_type.find("video/") == 0) {
            resource_type = "video";
        }
        */

        /*
         * AVIF conversion is disabled, so resource_type is determined
         * directly from the uploaded file's content type.
         */

        if (original_content_type.find("image/") == 0) {
            resource_type = "image";
        }
        else if (original_content_type.find("video/") == 0) {
            resource_type = "video";
        }

        std::map<std::string, std::string> additional_params = {
            {
                "folder",
                "projects/" + *user_id
            },
            {
                "unique_filename",
                "true"
            }
        };

        std::string response_body;

        try {
            LOG_INFO()
                << "Uploading to Cloudinary:"
                << " folder=projects/" << *user_id
                << " resource_type=" << resource_type
                << " file=" << upload_filename
                << " content_type=" << upload_content_type
                << " size=" << upload_buffer.size();

            response_body =
                cloudinary_client_.UploadFile(
                    upload_buffer,
                    upload_filename,
                    upload_content_type,
                    additional_params,
                    resource_type
                );
        }
        catch (const std::exception& e) {
            LOG_ERROR()
                << "Cloudinary upload failed: "
                << e.what();

            return return_error(
                userver::server::http::HttpStatus::kInternalServerError,
                "CLOUDINARY_ERROR",
                std::string("Upload failed: ") + e.what()
            );
        }

        auto json =
            userver::formats::json::FromString(
                response_body
            );

        const auto public_id =
            json["public_id"].As<std::string>("");

        LOG_INFO()
            << "Cloudinary upload success: public_id="
            << public_id;

        auto* item =
            batch_res.add_items();

        /*
         * Store the original Cloudinary secure_url.
         *
         * Do not add delivery transformations here.
         *
         * Example:
         *
         * https://res.cloudinary.com/.../image/upload/v123/file.png
         *
         * instead of:
         *
         * https://res.cloudinary.com/.../image/upload/f_auto,q_auto/v123/file.png
         *
         * Delivery transformations can be added later when
         * constructing the API response if needed.
         */

        const std::string media_url =
            json["secure_url"].As<std::string>(
                json["url"].As<std::string>("")
            );

        item->set_url(media_url);

        item->set_public_id(public_id);

        const auto cloudinary_resource_type =
            json["resource_type"].As<std::string>(
                resource_type
            );

        item->set_resource_type(
            cloudinary_resource_type
        );

        if (!public_id.empty()) {
            try {
                _media.InsertOrphan(
                    public_id,
                    *user_id,
                    cloudinary_resource_type
                );
            }
            catch (const std::exception& e) {
                LOG_ERROR()
                    << "Failed to track media upload "
                    << public_id
                    << ": "
                    << e.what();

                return return_error(
                    userver::server::http::HttpStatus::kInternalServerError,
                    "DB_ERROR",
                    "Failed to record uploaded media"
                );
            }
        }

        item->set_type(
            MediaTypeFromCloudinary(
                cloudinary_resource_type,
                upload_content_type
            )
        );

        item->mutable_id()->set_value(
            userver::utils::generators::GenerateUuid()
        );
    }

    res.SetContentType("application/protobuf");

    return batch_res.SerializeAsString();
}

}  // namespace priemman::handlers::media