#ifndef KOLOSAL_PARSE_DOCUMENT_ROUTE_HPP
#define KOLOSAL_PARSE_DOCUMENT_ROUTE_HPP

#include "../route_interface.hpp"
#include <json.hpp>
#include <string>
#include <mutex>
#include <thread>

namespace kolosal
{
    class ParseDocumentRoute : public IRoute
    {
    public:
        bool match(const std::string &method, const std::string &path) override;
        void handle(SocketType sock, const std::string &body) override;

    private:
        enum class DocumentType {
            PDF,
            DOCX,
            HTML
        };

        // Thread-local storage for current path
        static thread_local std::string current_path_;
        mutable std::mutex path_mutex_;

        DocumentType getDocumentType(const std::string &path);
        std::string getDataKey(DocumentType type);
        std::string getLogPrefix(DocumentType type);
        void sendJsonResponse(SocketType sock, const nlohmann::json &response, int status_code = 200);
        bool parseRequest(const std::string &body, nlohmann::json &request, SocketType sock);
        bool validateDocumentData(const nlohmann::json &request, const std::string &data_key, SocketType sock);
        std::vector<unsigned char> decodeBase64Data(const std::string &base64_data, SocketType sock);
        void sendOptionsResponse(SocketType sock, const std::string &endpoint_name, const std::string &description);
    };

} // namespace kolosal

#endif // KOLOSAL_PARSE_DOCUMENT_ROUTE_HPP
