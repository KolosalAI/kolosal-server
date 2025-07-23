#include "kolosal/routes/documents_upload_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/retrieval/add_document_types.hpp"
#include "kolosal/retrieval/document_service.hpp"
#include "kolosal/retrieval/parse_pdf.hpp"
#include <json.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>

using json = nlohmann::json;

namespace kolosal::routes
{

namespace {
    // Base64 decoding table
    static const unsigned char base64_decode_table[256] = {
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
        64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
        64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64};

    // Base64 decoding function
    std::vector<unsigned char> base64_decode(const std::string &encoded)
    {
        std::vector<unsigned char> decoded;

        if (encoded.empty())
        {
            return decoded;
        }

        // Remove whitespace and padding
        std::string clean_encoded;
        for (char c : encoded)
        {
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
            {
                clean_encoded += c;
            }
        }

        // Remove padding
        while (!clean_encoded.empty() && clean_encoded.back() == '=')
        {
            clean_encoded.pop_back();
        }

        if (clean_encoded.empty())
        {
            return decoded;
        }

        size_t input_length = clean_encoded.length();
        size_t output_length = ((input_length * 3) / 4);
        decoded.reserve(output_length);

        unsigned int buffer = 0;
        int bits_collected = 0;

        for (char c : clean_encoded)
        {
            unsigned char value = base64_decode_table[static_cast<unsigned char>(c)];
            if (value == 64)
            {
                // Invalid character
                throw std::invalid_argument("Invalid base64 character");
            }

            buffer = (buffer << 6) | value;
            bits_collected += 6;

            if (bits_collected >= 8)
            {
                decoded.push_back(static_cast<unsigned char>((buffer >> (bits_collected - 8)) & 0xFF));
                bits_collected -= 8;
            }
        }

        return decoded;
    }
}

DocumentsUploadRoute::DocumentsUploadRoute()
{
    ServerLogger::logInfo("DocumentsUploadRoute initialized");
}

DocumentsUploadRoute::~DocumentsUploadRoute() = default;

bool DocumentsUploadRoute::match(const std::string& method, const std::string& path)
{
    return method == "POST" && (path == "/documents/upload" || path == "/api/v1/documents/upload");
}

void DocumentsUploadRoute::handle(SocketType sock, const std::string& body)
{
    try
    {
        ServerLogger::logInfo("Document upload request received");
        
        if (body.empty())
        {
            sendErrorResponse(sock, 400, "Request body is empty");
            return;
        }

        json request_data;
        try
        {
            request_data = json::parse(body);
        }
        catch (const json::parse_error& ex)
        {
            sendErrorResponse(sock, 400, "Invalid JSON: " + std::string(ex.what()));
            return;
        }

        // Expected format:
        // {
        //   "file_content": "base64_encoded_content_or_text",
        //   "filename": "document.txt",
        //   "content_type": "text/plain",
        //   "metadata": { ... }
        // }

        if (!request_data.contains("file_content"))
        {
            sendErrorResponse(sock, 400, "file_content is required", "missing_parameter", "file_content");
            return;
        }

        std::string file_content = request_data["file_content"];
        std::string filename = request_data.value("filename", "uploaded_document.txt");
        std::string content_type = request_data.value("content_type", "text/plain");
        json metadata = request_data.value("metadata", json::object());

        // Add filename to metadata
        metadata["source"] = filename;
        metadata["upload_timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        // Process the file content based on content type
        std::string text_content;
        if (content_type == "text/plain" || content_type == "application/json")
        {
            text_content = file_content;
        }
        else if (content_type == "application/pdf")
        {
            // For PDF files, decode base64 and parse
            try 
            {
                // Base64 decode the content
                std::vector<unsigned char> decoded_data = base64_decode(file_content);
                
                // Use PDF parser to extract text
                ::retrieval::ParseResult result = ::retrieval::DocumentParser::parse_pdf_from_bytes(
                    decoded_data.data(), decoded_data.size(), 
                    ::retrieval::PDFParseMethod::Fast, "eng", nullptr);
                
                if (result.success) {
                    text_content = result.text;
                } else {
                    sendErrorResponse(sock, 400, "Failed to parse PDF: " + result.error_message, "pdf_parse_error", "");
                    return;
                }
            }
            catch (const std::exception& e)
            {
                sendErrorResponse(sock, 400, "Failed to decode or parse PDF: " + std::string(e.what()), "pdf_decode_error", "");
                return;
            }
        }
        else
        {
            // For other types, assume base64 encoded and decode
            try {
                std::vector<unsigned char> decoded_data = base64_decode(file_content);
                text_content = std::string(decoded_data.begin(), decoded_data.end());
            } catch (const std::exception& e) {
                // If base64 decoding fails, treat as plain text
                text_content = file_content;
            }
        }

        // Create add documents request
        kolosal::retrieval::AddDocumentsRequest addRequest;
        kolosal::retrieval::Document document;
        document.text = text_content;
        
        // Convert json metadata to unordered_map<string, json>
        std::unordered_map<std::string, nlohmann::json> metadata_map;
        if (metadata.is_object()) {
            for (auto& [key, value] : metadata.items()) {
                metadata_map[key] = value;
            }
        }
        document.metadata = metadata_map;
        
        addRequest.documents.push_back(document);
        addRequest.collection_name = "documents";

        // Validate request
        if (!addRequest.validate())
        {
            sendErrorResponse(sock, 400, "Invalid document data");
            return;
        }

        // Submit to document service
        try
        {
            auto& serverAPI = ServerAPI::instance();
            auto& documentService = serverAPI.getDocumentService();
            
            auto future_response = documentService.addDocuments(addRequest);
            auto add_response = future_response.get();
            
            // Create response
            json response = {
                {"success", true},
                {"message", "File uploaded and indexed successfully"},
                {"filename", filename},
                {"content_type", content_type},
                {"documents_processed", add_response.successful_count},
                {"failed_count", add_response.failed_count}
            };

            if (add_response.successful_count > 0 && !add_response.results.empty())
            {
                response["document_id"] = add_response.results[0].id;
            }

            sendSuccessResponse(sock, response);
        }
        catch (const std::runtime_error& e)
        {
            ServerLogger::logError("DocumentService error in upload: %s", e.what());
            sendErrorResponse(sock, 503, "Document service not available: " + std::string(e.what()));
            return;
        }
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("Error in DocumentsUploadRoute::handle: %s", ex.what());
        sendErrorResponse(sock, 500, "Internal server error");
    }
}

void DocumentsUploadRoute::sendErrorResponse(SocketType sock, int status, const std::string& message,
                                           const std::string& error_type, const std::string& param)
{
    json error_response;
    error_response["success"] = false;
    error_response["error"] = {
        {"type", error_type},
        {"message", message},
        {"code", status}
    };
    if (!param.empty())
    {
        error_response["error"]["param"] = param;
    }

    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
    };
    send_response(sock, status, error_response.dump(), headers);
}

void DocumentsUploadRoute::sendSuccessResponse(SocketType sock, const nlohmann::json& data)
{
    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
    };
    send_response(sock, 200, data.dump(), headers);
}

} // namespace kolosal::routes
