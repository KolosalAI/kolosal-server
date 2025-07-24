#include "kolosal/routes/parse_pdf_file_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/retrieval/parse_pdf.hpp"
#include <json.hpp>
#include <iostream>
#include <sstream>
#include <thread>
#include <filesystem>

using json = nlohmann::json;

namespace kolosal
{
    namespace
    {
        void sendJsonResponse(SocketType sock, const json &response, int status_code = 200)
        {
            std::string status_text;
            switch (status_code)
            {
            case 200:
                status_text = "OK";
                break;
            case 400:
                status_text = "Bad Request";
                break;
            case 404:
                status_text = "Not Found";
                break;
            case 500:
                status_text = "Internal Server Error";
                break;
            default:
                status_text = "Unknown";
                break;
            }

            std::string response_str = response.dump();
            std::ostringstream http_response;
            http_response << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
            http_response << "Content-Type: application/json\r\n";
            http_response << "Content-Length: " << response_str.length() << "\r\n";
            http_response << "Access-Control-Allow-Origin: *\r\n";
            http_response << "Access-Control-Allow-Methods: POST, OPTIONS\r\n";
            http_response << "Access-Control-Allow-Headers: Content-Type\r\n";
            http_response << "\r\n";
            http_response << response_str;

            std::string full_response = http_response.str();
            send(sock, full_response.c_str(), static_cast<int>(full_response.length()), 0);
        }
    }

    bool ParsePDFFileRoute::match(const std::string &method, const std::string &path)
    {
        return (method == "POST" && path == "/parse-pdf");
    }

    void ParsePDFFileRoute::handle(SocketType sock, const std::string &body)
    {
        try
        {
            ServerLogger::logInfo("[Thread %u] Received PDF file parse request", std::this_thread::get_id());

            // Handle OPTIONS request for CORS
            if (body.empty())
            {
                json response = {
                    {"message", "PDF file parse endpoint ready"},
                    {"methods", {"POST"}},
                    {"description", "Send file path to parse PDF text"}};
                sendJsonResponse(sock, response, 200);
                return;
            }

            // Parse JSON request
            json request;
            try
            {
                request = json::parse(body);
            }
            catch (const json::parse_error &e)
            {
                json error_response = {
                    {"error", "Invalid JSON format"},
                    {"details", e.what()}};
                sendJsonResponse(sock, error_response, 400);
                return;
            }

            // Validate required fields
            if (!request.contains("file_path") || !request["file_path"].is_string())
            {
                json error_response = {
                    {"error", "Missing or invalid 'file_path' field"},
                    {"details", "Expected file path as string"}};
                sendJsonResponse(sock, error_response, 400);
                return;
            }

            std::string file_path = request["file_path"];
            if (file_path.empty())
            {
                json error_response = {
                    {"error", "Empty file path"},
                    {"details", "File path cannot be empty"}};
                sendJsonResponse(sock, error_response, 400);
                return;
            }

            // Check if file exists
            if (!std::filesystem::exists(file_path))
            {
                json error_response = {
                    {"error", "Failed to parse PDF: " + file_path},
                    {"details", "File not found"}};
                sendJsonResponse(sock, error_response, 404);
                return;
            }

            // Optional parameters
            std::string method_str = request.value("method", "fast");
            std::string language = request.value("language", "eng");

            ServerLogger::logInfo("Parsing PDF file: %s using method: %s", file_path.c_str(), method_str.c_str());

            // Parse the PDF
            retrieval::PDFParseMethod parse_method;
            if (method_str == "fast") {
                parse_method = retrieval::PDFParseMethod::Fast;
            } else if (method_str == "ocr") {
                parse_method = retrieval::PDFParseMethod::OCR;
            } else if (method_str == "visual") {
                parse_method = retrieval::PDFParseMethod::Visual;
            } else {
                parse_method = retrieval::PDFParseMethod::Fast;
            }

            // Parse PDF from file
            retrieval::ParseResult result = retrieval::DocumentParser::parse_pdf(
                file_path, parse_method, language);

            // Create response
            json response;
            if (result.success)
            {
                response = {
                    {"success", true},
                    {"text", result.text},
                    {"pages_processed", result.pages_processed},
                    {"method", method_str},
                    {"language", language},
                    {"file_path", file_path}};
                ServerLogger::logInfo("PDF parsing completed successfully. Pages: %zu, Text length: %zu",
                                      result.pages_processed, result.text.length());
            }
            else
            {
                response = {
                    {"success", false},
                    {"error", "Failed to parse PDF: " + file_path},
                    {"details", result.error_message},
                    {"pages_processed", result.pages_processed},
                    {"method", method_str},
                    {"language", language},
                    {"file_path", file_path}};
                ServerLogger::logError("PDF parsing failed: %s", result.error_message.c_str());
            }

            sendJsonResponse(sock, response, result.success ? 200 : 404);
        }
        catch (const std::exception &e)
        {
            ServerLogger::logError("Exception in ParsePDFFileRoute::handle: %s", e.what());
            json error_response = {
                {"success", false},
                {"error", "Internal server error"},
                {"details", e.what()}};
            sendJsonResponse(sock, error_response, 500);
        }
    }

} // namespace kolosal
