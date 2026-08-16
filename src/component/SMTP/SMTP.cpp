#include "SMTP.hpp"
#include <chrono>
#include <exception>
#include <string>
#include <userver/engine/deadline.hpp>
#include <userver/engine/io/sockaddr.hpp>
#include <userver/engine/io/socket.hpp>

namespace daberdev::components {

    SMTPClientComponent::SMTPClientComponent(
        const userver::components::ComponentConfig & config,
        const userver::components::ComponentContext & context
    ): userver::components::LoggableComponentBase(config, context),
        m_host {config["host"].As<std::string>()},
        m_port {config["port"].As<uint16_t>()},
        m_email {config["email"].As<std::string>()},
        m_password {config["password"].As<std::string>()},
        _resolver{&context.FindComponent<userver::clients::dns::Component>("dns-client").GetResolver()},
        m_background_tasks{context.GetTaskProcessor("main-task-processor")} {}

    void SMTPClientComponent::SendEmailAsync(
        std::string to,
        std::string subject,
        std::string body
    ) {
        m_background_tasks.AsyncDetach(
            "smtp/send-email",
            [
                this,
                to = std::move(to),
                subject = std::move(subject),
                body = std::move(body)
            ] {
                const bool success = SendEmail(to, subject, body);

                if (!success) {
                    LOG_WARNING()
                        << "Background email failed"
                        << userver::logging::LogExtra{
                                {"recipient", to},
                                {"subject", subject},
                            };
                }
            }
        );
    }

    bool SMTPClientComponent::SendEmail(
        const std::string& to,
        const std::string& subject,
        const std::string& body
    ) {
        using userver::engine::Deadline;

        constexpr auto kIoTimeout = std::chrono::seconds{10};

        const auto make_deadline = [&kIoTimeout] {
            return Deadline::FromDuration(kIoTimeout);
        };

        try {
            auto send_data = [&](auto& stream, std::string_view data) {
                const auto bytes_sent = stream.SendAll(
                    data.data(),
                    data.size(),
                    make_deadline()
                );

                if (bytes_sent != data.size()) {
                    throw std::runtime_error(fmt::format(
                        "SMTP send incomplete: sent {} of {} bytes",
                        bytes_sent,
                        data.size()
                    ));
                }
            };

            auto is_final_response_line = [](std::string_view line) {
                return line.size() >= 4 &&
                        std::isdigit(static_cast<unsigned char>(line[0])) &&
                        std::isdigit(static_cast<unsigned char>(line[1])) &&
                        std::isdigit(static_cast<unsigned char>(line[2])) &&
                        line[3] == ' ';
            };

            auto read_response = [&](auto& stream) -> std::string {
                std::array<char, 1024> buffer{};
                std::string response;
                std::size_t scan_position = 0;

                while (true) {
                    const auto bytes_read = stream.RecvSome(
                        buffer.data(),
                        buffer.size(),
                        make_deadline()
                    );

                    if (bytes_read == 0) {
                        throw std::runtime_error(
                            "SMTP server closed the connection"
                        );
                    }

                    response.append(buffer.data(), bytes_read);

                    while (true) {
                        const auto line_end =
                            response.find("\r\n", scan_position);

                        if (line_end == std::string::npos) {
                            break;
                        }

                        const std::string_view line{
                            response.data() + scan_position,
                            line_end - scan_position
                        };

                        if (is_final_response_line(line)) {
                            return response;
                        }

                        scan_position = line_end + 2;
                    }
                }
            };

            auto expect_response = [&](auto& stream, int expected_code) {
                const auto response = read_response(stream);

                LOG_DEBUG() << "SMTP response: " << response;

                if (response.size() < 3) {
                    throw std::runtime_error(
                        "Invalid SMTP response: " + response
                    );
                }

                const int actual_code = std::stoi(response.substr(0, 3));

                if (actual_code != expected_code) {
                    throw std::runtime_error(fmt::format(
                        "Unexpected SMTP response. Expected {}, received {}: {}",
                        expected_code,
                        actual_code,
                        response
                    ));
                }

                return response;
            };

            auto resolved = _resolver->Resolve(m_host, make_deadline());
            if (resolved.empty()) {
                throw std::runtime_error("DNS resolution failed for host: " + m_host);
            }

            // Prioritaskan IPv4 karena banyak environment (termasuk WSL) tidak punya
            // konektivitas IPv6 keluar, meski DNS tetap mengembalikan record AAAA.
            userver::engine::io::Sockaddr address;
            bool found_ipv4 = false;

            for (const auto& addr : resolved) {
                if (addr.Domain() == userver::engine::io::AddrDomain::kInet) {
                    address = addr;
                    found_ipv4 = true;
                    break;
                }
            }

            if (!found_ipv4) {
                // fallback ke alamat pertama (kemungkinan IPv6) kalau memang tidak ada IPv4
                address = resolved.front();
            }

            address.SetPort(m_port);

            userver::engine::io::Socket socket{
                address.Domain(),
                userver::engine::io::SocketType::kStream
            };

            if (!socket.IsValid()) {
                throw std::runtime_error("Failed to initialize SMTP socket");
            }

            socket.Connect(address, make_deadline());

            expect_response(socket, 220);

            send_data(socket, "EHLO localhost\r\n");
            expect_response(socket, 250);

            send_data(socket, "STARTTLS\r\n");
            expect_response(socket, 220);

            auto tls_socket =
                userver::engine::io::TlsWrapper::StartTlsClient(
                    std::move(socket),
                    m_host,
                    make_deadline()
                );

            send_data(tls_socket, "EHLO localhost\r\n");
            expect_response(tls_socket, 250);

            send_data(tls_socket, "AUTH LOGIN\r\n");
            expect_response(tls_socket, 334);

            const auto encoded_email =
                userver::crypto::base64::Base64Encode(m_email);

            send_data(tls_socket, encoded_email + "\r\n");
            expect_response(tls_socket, 334);

            const auto encoded_password =
                userver::crypto::base64::Base64Encode(m_password);

            send_data(tls_socket, encoded_password + "\r\n");
            expect_response(tls_socket, 235);

            send_data(
                tls_socket,
                fmt::format("MAIL FROM:<{}>\r\n", m_email)
            );
            expect_response(tls_socket, 250);

            send_data(
                tls_socket,
                fmt::format("RCPT TO:<{}>\r\n", to)
            );
            expect_response(tls_socket, 250);

            send_data(tls_socket, "DATA\r\n");
            expect_response(tls_socket, 354);

            const std::string email_payload = fmt::format(
                "From: <{}>\r\n"
                "To: <{}>\r\n"
                "Subject: {}\r\n"
                "MIME-Version: 1.0\r\n"
                "Content-Type: text/html; charset=UTF-8\r\n"
                "Content-Transfer-Encoding: 8bit\r\n"
                "\r\n"
                "{}\r\n"
                ".\r\n",
                m_email,
                to,
                subject,
                body
            );

            send_data(tls_socket, email_payload);

            expect_response(tls_socket, 250);

            send_data(tls_socket, "QUIT\r\n");
            expect_response(tls_socket, 221);

            LOG_INFO() << "Email successfully sent to " << to;
            return true;

        } catch (const std::exception& error) {
            LOG_ERROR() << "Failed to send email: " << error.what();
            return false;
        }
    }

    userver::yaml_config::Schema SMTPClientComponent::GetStaticConfigSchema() {
        return userver::yaml_config::MergeSchemas<userver::components::LoggableComponentBase>(R"(
        type: object
        description: SmtpClientComponent config
        additionalProperties: false
        properties:
            host:
                type: string
                description: Alamat server SMTP
            port:
                type: integer
                description: Port server SMTP
            email:
                type: string
                description: Email pengirim
            password:
                type: string
                description: Password aplikasi
        )");
    }
}
