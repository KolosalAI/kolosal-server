#include "kolosal/routes/retrieval/parse_document_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/retrieval/parse_pdf.hpp"
#include "kolosal/retrieval/parse_docx.hpp"
#include "kolosal/retrieval/parse_html.hpp"
#include <json.hpp>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>
#include <algorithm>
#include <memory>
#include <iomanip>
#include <mutex>

// Use the existing base64 utility from llama.cpp
#include "../../external/llama.cpp/common/base64.hpp"

using json = nlohmann::json;

namespace kolosal
{
    // Thread-local storage for current path
    thread_local std::string ParseDocumentRoute::current_path_;

    namespace
    {
        // Helper function to convert method string to enum
        retrieval::PDFParseMethod parseMethodFromString(const std::string &method_str)
        {
            std::string lower_method = method_str;
            std::transform(lower_method.begin(), lower_method.end(), lower_method.begin(), ::tolower);

            if (lower_method == "fast")
            {
                return retrieval::PDFParseMethod::Fast;
            }
            else if (lower_method == "ocr")
            {
                return retrieval::PDFParseMethod::OCR;
            }
            else if (lower_method == "visual")
            {
                return retrieval::PDFParseMethod::Visual;
            }
            else
            {
                return retrieval::PDFParseMethod::Fast; // Default fallback
            }
        }
    }

    bool ParseDocumentRoute::match(const std::string &method, const std::string &path)
    {
        bool matches = method == "POST" && 
                      (path == "/parse_pdf" || path == "/parse_docx" || path == "/parse_html");
        
        if (matches)
        {
            // Store the path in thread-local storage (thread-safe)
            current_path_ = path;
        }
        
        return matches;
    }

    ParseDocumentRoute::DocumentType ParseDocumentRoute::getDocumentType(const std::string &path)
    {
        if (path == "/parse_pdf") return DocumentType::PDF;
        if (path == "/parse_docx") return DocumentType::DOCX;
        if (path == "/parse_html") return DocumentType::HTML;
        throw std::invalid_argument("Unknown document type for path: " + path);
    }

    std::string ParseDocumentRoute::getDataKey(DocumentType type)
    {
        switch (type)
        {
            case DocumentType::PDF: return "data";      // Match original PDF route
            case DocumentType::DOCX: return "data";     // Match original DOCX route  
            case DocumentType::HTML: return "html";     // Match original HTML route
        }
        return "";
    }

    std::string ParseDocumentRoute::getLogPrefix(DocumentType type)
    {
        switch (type)
        {
            case DocumentType::PDF: return "PDF";
            case DocumentType::DOCX: return "DOCX";
            case DocumentType::HTML: return "HTML";
        }
        return "";
    }

    void ParseDocumentRoute::sendJsonResponse(SocketType sock, const nlohmann::json &response, int status_code)
    {
        std::string response_str = response.dump();
        std::map<std::string, std::string> headers = {
            {"Content-Type", "application/json"},
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "POST, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
        };
        send_response(sock, status_code, response_str, headers);
    }

    void ParseDocumentRoute::sendOptionsResponse(SocketType sock, const std::string &endpoint_name, const std::string &description)
    {
        json response = {
            {"message", endpoint_name + " endpoint ready"},
            {"methods", {"POST"}},
            {"description", description}
        };
        sendJsonResponse(sock, response, 200);
    }

    bool ParseDocumentRoute::parseRequest(const std::string &body, nlohmann::json &request, SocketType sock)
    {
        if (body.empty())
        {
            json errorResponse;
            errorResponse["success"] = false;
            errorResponse["error"] = "Request body is empty";
            sendJsonResponse(sock, errorResponse, 400);
            return false;
        }

        try
        {
            request = json::parse(body);
            return true;
        }
        catch (const json::parse_error &ex)
        {
            ServerLogger::logError("[Thread %u] JSON parsing error: %s", std::this_thread::get_id(), ex.what());
            json errorResponse;
            errorResponse["success"] = false;
            errorResponse["error"] = "Invalid JSON format";
            errorResponse["details"] = ex.what();
            sendJsonResponse(sock, errorResponse, 400);
            return false;
        }
    }

    bool ParseDocumentRoute::validateDocumentData(const nlohmann::json &request, const std::string &data_key, SocketType sock)
    {
        if (!request.contains(data_key) || !request[data_key].is_string())
        {
            json error_response;
            error_response["success"] = false;
            error_response["error"] = "Missing or invalid '" + data_key + "' field";
            
            if (data_key == "data")
            {
                error_response["details"] = "Expected base64-encoded document data as string";
            }
            else if (data_key == "html")
            {
                error_response["details"] = "Expected HTML content as string";
            }
            
            sendJsonResponse(sock, error_response, 400);
            return false;
        }

        if (request[data_key].get<std::string>().empty())
        {
            json error_response;
            error_response["success"] = false;
            
            if (data_key == "data")
            {
                error_response["error"] = "Empty document data";
                error_response["details"] = "Document data cannot be empty";
            }
            else if (data_key == "html")
            {
                error_response["error"] = "Missing or invalid 'html' field in request";
            }
            
            sendJsonResponse(sock, error_response, 400);
            return false;
        }

        return true;
    }

    std::vector<unsigned char> ParseDocumentRoute::decodeBase64Data(const std::string &base64_data, SocketType sock)
    {
        try
        {
            // Use the existing base64 utility
            std::string decoded_str = base64::decode(base64_data);
            std::vector<unsigned char> decoded_data(decoded_str.begin(), decoded_str.end());
            
            if (decoded_data.empty())
            {
                json error_response;
                error_response["success"] = false;
                error_response["error"] = "Empty decoded document data";
                error_response["details"] = "Decoded document data is empty";
                sendJsonResponse(sock, error_response, 400);
                return {};
            }
            
            return decoded_data;
        }
        catch (const std::exception &ex)
        {
            json error_response;
            error_response["success"] = false;
            error_response["error"] = "Failed to decode base64 data";
            error_response["details"] = ex.what();
            sendJsonResponse(sock, error_response, 400);
            return {};
        }
    }

    void ParseDocumentRoute::handle(SocketType sock, const std::string &body)
    {
        try
        {
            // Get document type from thread-local storage
            DocumentType docType = getDocumentType(current_path_);
            std::string data_key = getDataKey(docType);
            std::string log_prefix = getLogPrefix(docType);

            ServerLogger::logInfo("[Thread %u] Received %s parse request", std::this_thread::get_id(), log_prefix.c_str());

            // Handle OPTIONS request for CORS (empty body indicates OPTIONS)
            if (body.empty())
            {
                std::string description;
                switch (docType)
                {
                    case DocumentType::PDF:
                        description = "Send base64-encoded PDF data to parse text";
                        break;
                    case DocumentType::DOCX:
                        description = "Send base64-encoded DOCX data to parse text";
                        break;
                    case DocumentType::HTML:
                        description = "Send HTML content to convert to Markdown";
                        break;
                }
                sendOptionsResponse(sock, log_prefix, description);
                return;
            }

            json request;
            if (!parseRequest(body, request, sock))
            {
                return;
            }

            // Validate document data
            if (!validateDocumentData(request, data_key, sock))
            {
                return;
            }

            json response;
            response["success"] = false;

            // Handle different document types
            switch (docType)
            {
                case DocumentType::HTML:
                {
                    std::string html_content = request[data_key].get<std::string>();
                    
                    ServerLogger::logDebug("[Thread %u] Converting HTML to Markdown (length: %zu)", 
                                          std::this_thread::get_id(), html_content.length());
                    
                    // Use the HTML parser class
                    retrieval::HtmlParser parser;
                    retrieval::HtmlParseResult result = parser.parseHtmlSync(html_content);
                    
                    if (result.success)
                    {
                        response["success"] = true;
                        response["markdown"] = result.markdown;
                        response["elements_processed"] = result.elements_processed;
                        
                        ServerLogger::logInfo("[Thread %u] Successfully converted HTML to Markdown", 
                                            std::this_thread::get_id());
                    }
                    else
                    {
                        response["error"] = result.error_message.empty() ? "Failed to parse HTML content" : result.error_message;
                        response["elements_processed"] = result.elements_processed;
                        ServerLogger::logError("[Thread %u] Error converting HTML to Markdown: %s",
                                             std::this_thread::get_id(), response["error"].get<std::string>().c_str());
                    }
                    break;
                }

                case DocumentType::PDF:
                case DocumentType::DOCX:
                {
                    // For PDF and DOCX, decode base64 data
                    std::string base64_data = request[data_key].get<std::string>();
                    std::vector<unsigned char> document_data = decodeBase64Data(base64_data, sock);
                    
                    if (document_data.empty())
                    {
                        return; // Error already sent by decodeBase64Data
                    }

                    ServerLogger::logInfo("Parsing %s data (size: %zu bytes)", log_prefix.c_str(), document_data.size());

                    if (docType == DocumentType::PDF)
                    {
                        // Handle PDF parsing
                        std::string method_str = "fast";
                        if (request.contains("method") && request["method"].is_string())
                        {
                            method_str = request["method"].get<std::string>();
                        }

                        std::string language = "eng";
                        if (request.contains("language") && request["language"].is_string())
                        {
                            language = request["language"].get<std::string>();
                        }

                        retrieval::PDFParseMethod parse_method = parseMethodFromString(method_str);

                        ServerLogger::logInfo("Parsing PDF data (size: %zu bytes) using method: %s, language: %s", 
                                            document_data.size(), method_str.c_str(), language.c_str());

                        // Progress callback (optional)
                        retrieval::ProgressCallback progress_cb = nullptr;
                        if (request.value("progress", false))
                        {
                            progress_cb = [](size_t current, size_t total)
                            {
                                ServerLogger::logInfo("PDF parsing progress: %zu/%zu pages", current, total);
                            };
                        }

                        auto result = retrieval::DocumentParser::parse_pdf_from_bytes(
                            document_data.data(), document_data.size(), parse_method, language, progress_cb
                        );

                        if (result.success)
                        {
                            response["success"] = true;
                            response["text"] = result.text;
                            response["pages_processed"] = result.pages_processed;
                            response["method"] = method_str;
                            response["language"] = language;
                            response["data_size_bytes"] = document_data.size();
                            
                            ServerLogger::logInfo("PDF parsing completed successfully. Pages: %zu, Text length: %zu",
                                                result.pages_processed, result.text.length());
                        }
                        else
                        {
                            response["error"] = result.error_message;
                            response["pages_processed"] = result.pages_processed;
                            response["method"] = method_str;
                            response["language"] = language;
                            response["data_size_bytes"] = document_data.size();
                            ServerLogger::logError("PDF parsing failed: %s", result.error_message.c_str());
                        }
                    }
                    else // DOCX
                    {
                        try
                        {
                            std::string parsed_text = retrieval::DOCXParser::parse_docx_from_bytes(
                                document_data.data(), document_data.size()
                            );
                            
                            response["success"] = true;
                            response["text"] = parsed_text;
                            response["pages_processed"] = 1; // DOCX doesn't have pages like PDF, so we'll use 1
                            response["data_size_bytes"] = document_data.size();
                            
                            ServerLogger::logInfo("DOCX parsing completed successfully. Text length: %zu",
                                                parsed_text.length());
                        }
                        catch (const std::exception &e)
                        {
                            response["error"] = e.what();
                            response["pages_processed"] = 1;
                            response["data_size_bytes"] = document_data.size();
                            ServerLogger::logError("DOCX parsing failed: %s", e.what());
                        }
                    }
                    break;
                }
            }

            sendJsonResponse(sock, response, response["success"].get<bool>() ? 200 : 500);
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("[Thread %u] Exception in document parsing: %s", std::this_thread::get_id(), ex.what());
            json error_response;
            error_response["success"] = false;
            error_response["error"] = "Internal server error";
            error_response["details"] = ex.what();
            sendJsonResponse(sock, error_response, 500);
        }
    }

} // namespace kolosal
