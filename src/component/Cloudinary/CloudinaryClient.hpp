#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include <userver/clients/http/client.hpp>
#include <userver/logging/log.hpp>

namespace priemman::cloudinary {

class Client {
public:
    Client(userver::clients::http::Client* http_client,
           const std::string& cloud_name,
           const std::string& api_key,
           const std::string& api_secret);

    // Generate signature untuk signed upload
    std::string GenerateSignature(const std::map<std::string, std::string>& params_to_sign) const;

    // Upload file ke Cloudinary (return JSON response sebagai string)
    // resource_type: "image", "video", "auto", "raw"
    std::string UploadFile(const std::string& file_data,
                          const std::string& filename,
                          const std::string& content_type,
                          const std::map<std::string, std::string>& additional_params = {},
                          const std::string& resource_type = "auto") const;

    // Delete media dari Cloudinary
    bool Delete(const std::string& public_id,
                const std::string& resource_type) const;
    // Overload lama: asumsikan image
    bool Delete(const std::string& public_id) const;

private:
    userver::clients::http::Client* http_client_;
    std::string cloud_name_;
    std::string api_key_;
    std::string api_secret_;
};

} // namespace priemman::cloudinary
