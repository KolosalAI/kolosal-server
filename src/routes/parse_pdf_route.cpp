#include "kolosal/routes/parse_pdf_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/retrieval/parse_pdf.hpp"
#include <json.hpp>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>
#include <algorithm>
#include <memory>
#include <iomanip>

using json = nlohmann::json;

namespace kolosal
{

    namespace
    {

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

        // Parse method from string
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
                return retrieval::PDFParseMethod::Fast; // Default
            }
        }

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

    bool ParsePDFRoute::match(const std::string &method, const std::string &path)
    {
        return (method == "POST" && path == "/parse_pdf");
    }

    void ParsePDFRoute::handle(SocketType sock, const std::string &body)
    {
        try
        {
            ServerLogger::logInfo("[Thread %u] Received PDF parse request", std::this_thread::get_id());

            // Handle OPTIONS request for CORS
            if (body.empty())
            {
                json response = {
                    {"message", "PDF parse endpoint ready"},
                    {"methods", {"POST"}},
                    {"description", "Send base64-encoded PDF data to parse text"}};
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
            if (!request.contains("data") || !request["data"].is_string())
            {
                json error_response = {
                    {"error", "Missing or invalid 'data' field"},
                    {"details", "Expected base64-encoded PDF data as string"}};
                sendJsonResponse(sock, error_response, 400);
                return;
            }

            std::string pdf_data_b64 = request["data"];
            if (pdf_data_b64.empty())
            {
                json error_response = {
                    {"error", "Empty PDF data"},
                    {"details", "PDF data cannot be empty"}};
                sendJsonResponse(sock, error_response, 400);
                return;
            }

            // Optional parameters
            std::string method_str = request.value("method", "fast");
            std::string language = request.value("language", "eng");

            // Decode base64 data
            std::vector<unsigned char> pdf_data;
            try
            {
                pdf_data = base64_decode(pdf_data_b64);
            }
            catch (const std::exception &e)
            {
                json error_response = {
                    {"error", "Failed to decode base64 data"},
                    {"details", e.what()}};
                sendJsonResponse(sock, error_response, 400);
                return;
            }

            if (pdf_data.empty())
            {
                json error_response = {
                    {"error", "Empty decoded PDF data"},
                    {"details", "Decoded PDF data is empty"}};
                sendJsonResponse(sock, error_response, 400);
                return;
            }

            ServerLogger::logInfo("Parsing PDF data (size: %zu bytes) using method: %s",
                                  pdf_data.size(), method_str.c_str());

            // Parse the PDF
            retrieval::PDFParseMethod parse_method = parseMethodFromString(method_str);

            // Progress callback (optional)
            retrieval::ProgressCallback progress_cb = nullptr;
            if (request.value("progress", false))
            {
                progress_cb = [](size_t current, size_t total)
                {
                    ServerLogger::logInfo("PDF parsing progress: %zu/%zu pages", current, total);
                };
            }

            // Parse PDF from bytes
            retrieval::ParseResult result = retrieval::DocumentParser::parse_pdf_from_bytes(
                pdf_data.data(), pdf_data.size(), parse_method, language, progress_cb);

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
                    {"data_size_bytes", pdf_data.size()}};
                ServerLogger::logInfo("PDF parsing completed successfully. Pages: %zu, Text length: %zu",
                                      result.pages_processed, result.text.length());
            }
            else
            {
                response = {
                    {"success", false},
                    {"error", result.error_message},
                    {"pages_processed", result.pages_processed},
                    {"method", method_str},
                    {"language", language},
                    {"data_size_bytes", pdf_data.size()}};
                ServerLogger::logError("PDF parsing failed: %s", result.error_message.c_str());
            }

            sendJsonResponse(sock, response, result.success ? 200 : 500);
        }
        catch (const std::exception &e)
        {
            ServerLogger::logError("Exception in ParsePdfRoute::handle: %s", e.what());
            json error_response = {
                {"success", false},
                {"error", "Internal server error"},
                {"details", e.what()}};
            sendJsonResponse(sock, error_response, 500);
        }
    }

} // namespace kolosal
