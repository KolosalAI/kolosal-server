#pragma once

#include "kolosal/routes/route_interface.hpp"
#include "kolosal/retrieval/document_service.hpp"
#include <memory>
#include <mutex>

namespace kolosal
{

class RetrieveRoute : public Route
{
public:
    RetrieveRoute();
    ~RetrieveRoute() override;

    bool match(const std::string& method, const std::string& path) override;
    void handle(SocketType sock, const std::string& body) override;

private:
    void handleRetrieve(SocketType sock, const std::string& body);
    void handleOptions(SocketType sock);
    void sendErrorResponse(SocketType sock, int status, const std::string& message,
                          const std::string& error_type = "request_error", const std::string& param = "");
    bool ensureDocumentService();

    std::string current_endpoint_;
    std::string current_method_;
    std::unique_ptr<kolosal::retrieval::DocumentService> document_service_;
    std::mutex service_mutex_;
    static std::atomic<long long> request_counter_;
};

} // namespace kolosal
