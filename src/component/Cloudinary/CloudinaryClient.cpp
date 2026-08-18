#include "CloudinaryClient.hpp"

#include <chrono>
#include <fmt/format.h>

#include <userver/clients/http/form.hpp>
#include <userver/crypto/hash.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value.hpp>

namespace priemman::cloudinary {

Client::Client(userver::clients::http::Client* http_client,
               const std::string& cloud_name,
               const std::string& api_key,
               const std::string& api_secret)
    : http_client_(http_client),
      cloud_name_(cloud_name),
      api_key_(api_key),
      api_secret_(api_secret) {}

std::string Client::GenerateSignature(const std::map<std::string, std::string>& params_to_sign) const {
    std::string to_sign;
    for (const auto& [key, value] : params_to_sign) {
        if (!to_sign.empty()) to_sign += "&";
        to_sign += key + "=" + value;
    }
    to_sign += api_secret_;
    return userver::crypto::hash::Sha1(to_sign);
}

std::string Client::UploadFile(const std::string& file_data,
                              const std::string& filename,
                              const std::string& content_type,
                              const std::map<std::string, std::string>& additional_params,
                              const std::string& resource_type) const {
    const auto timestamp = std::to_string(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );

    // Build params untuk signature - SEMUA parameter kecuali file dan api_key
    std::map<std::string, std::string> params_to_sign = {
        {"timestamp", timestamp}
    };

    // Tambahkan semua additional_params ke signature
    for (const auto& [key, value] : additional_params) {
        params_to_sign[key] = value;
    }

    const auto signature = GenerateSignature(params_to_sign);

    // Gunakan resource_type di URL
    auto url = fmt::format("https://api.cloudinary.com/v1_1/{}/{}/upload",
                          cloud_name_, resource_type);

    auto form = userver::clients::http::Form();
    auto file_buffer = std::make_shared<std::string>(file_data);
    form.AddBuffer("file", filename, file_buffer, content_type);
    form.AddContent("api_key", api_key_);
    form.AddContent("timestamp", timestamp);
    form.AddContent("signature", signature);

    // Tambahkan additional_params ke form
    for (const auto& [key, value] : additional_params) {
        form.AddContent(key, value);
    }

    // Log untuk debug
    LOG_DEBUG() << "Cloudinary upload URL: " << url;
    LOG_DEBUG() << "Cloudinary params: timestamp=" << timestamp
                << ", folder=" << (additional_params.count("folder") ? additional_params.at("folder") : "none")
                << ", unique_filename=" << (additional_params.count("unique_filename") ? additional_params.at("unique_filename") : "none");

    auto response = http_client_->CreateRequest()
        .post(url, std::move(form))
        .timeout(std::chrono::seconds(60))
        .retry(2)
        .perform();

    if (response->status_code() != 200) {
        auto error_body = response->body();
        LOG_ERROR() << "Cloudinary upload failed: status=" << response->status_code()
                    << ", body=" << error_body;
        throw std::runtime_error(
            fmt::format("Cloudinary upload failed (status {}): {}",
                       response->status_code(), error_body)
        );
    }

    return response->body();
}

bool Client::Delete(const std::string& public_id,
                    const std::string& resource_type) const {
    const auto timestamp = std::to_string(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );

    std::map<std::string, std::string> params = {
        {"public_id", public_id},
        {"timestamp", timestamp}
    };
    const auto signature = GenerateSignature(params);

    auto url = fmt::format("https://api.cloudinary.com/v1_1/{}/{}/destroy",
                           cloud_name_, resource_type);

    auto form = userver::clients::http::Form();
    form.AddContent("public_id", public_id);
    form.AddContent("api_key", api_key_);
    form.AddContent("timestamp", timestamp);
    form.AddContent("signature", signature);

    auto response = http_client_->CreateRequest()
        .post(url, std::move(form))
        .timeout(std::chrono::seconds(10))
        .perform();

    if (response->status_code() != 200) {
        LOG_ERROR() << "Cloudinary delete failed: status=" << response->status_code()
                    << ", body=" << response->body();
        return false;
    }

    auto json = userver::formats::json::FromString(response->body());
    const auto result = json["result"].As<std::string>();
    if (result == "not found") {
        // Aset sudah tidak ada (mis. sudah dihapus lewat jalur lain) —
        // tujuan tercapai, anggap sukses.
        LOG_INFO() << "Cloudinary delete: public_id=" << public_id
                   << " already gone";
        return true;
    }
    return result == "ok";
}

bool Client::Delete(const std::string& public_id) const {
    return Delete(public_id, "image");
}

} // namespace priemman::cloudinary
