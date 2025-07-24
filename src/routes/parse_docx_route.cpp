#include "kolosal/routes/parse_docx_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/retrieval/parse_docx.hpp"
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

    bool ParseDOCXRoute::match(const std::string &method, const std::string &path)
    {
        return (method == "POST" && path == "/parse_docx");
    }

    void ParseDOCXRoute::handle(SocketType sock, const std::string &body)
    {        try
        {
            ServerLogger::logInfo("[Thread %u] Received DOCX parse request", std::this_thread::get_id());

            // Handle OPTIONS request for CORS
            if (body.empty())
            {
                json response = {
                    {"message", "DOCX parse endpoint ready"},
                    {"methods", {"POST"}},
                    {"description", "Send base64-encoded DOCX data to parse text"}};
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
                    {"details", "Expected base64-encoded DOCX data as string"}};
                sendJsonResponse(sock, error_response, 400);
                return;
            }

            std::string docx_data_b64 = request["data"];
            if (docx_data_b64.empty())
            {
                json error_response = {
                    {"error", "Empty DOCX data"},
                    {"details", "DOCX data cannot be empty"}};
                sendJsonResponse(sock, error_response, 400);
                return;
            }

            // Decode base64 data
            std::vector<unsigned char> docx_data;
            try
            {
                docx_data = base64_decode(docx_data_b64);
            }
            catch (const std::exception &e)
            {
                json error_response = {
                    {"error", "Failed to decode base64 data"},
                    {"details", e.what()}};
                sendJsonResponse(sock, error_response, 400);
                return;
            }

            if (docx_data.empty())
            {
                json error_response = {
                    {"error", "Empty decoded DOCX data"},
                    {"details", "Decoded DOCX data is empty"}};
                sendJsonResponse(sock, error_response, 400);
                return;
            }

            ServerLogger::logInfo("Parsing DOCX data (size: %zu bytes)",
                                  docx_data.size());

            // Parse DOCX from bytes using the DOCXParser
            std::string parsed_text;
            bool parsing_success = false;
            size_t pages_processed = 0;
            std::string error_message = "";

            try
            {
                parsed_text = retrieval::DOCXParser::parse_docx_from_bytes(
                    docx_data.data(), docx_data.size());
                parsing_success = true;
                pages_processed = 1; // DOCX doesn't have pages like PDF, so we'll use 1
            }
            catch (const std::exception &e)
            {
                parsing_success = false;
                error_message = e.what();
                ServerLogger::logError("DOCX parsing failed: %s", e.what());
            }

            // Create response
            json response;
            if (parsing_success)
            {
                response = {
                    {"success", true},
                    {"text", parsed_text},
                    {"pages_processed", pages_processed},
                    {"data_size_bytes", docx_data.size()}};
                ServerLogger::logInfo("DOCX parsing completed successfully. Text length: %zu",
                                      parsed_text.length());
            }
            else
            {
                response = {
                    {"success", false},
                    {"error", error_message},
                    {"pages_processed", pages_processed},
                    {"data_size_bytes", docx_data.size()}};
                ServerLogger::logError("DOCX parsing failed: %s", error_message.c_str());
            }

            sendJsonResponse(sock, response, parsing_success ? 200 : 500);
        }
        catch (const std::exception &e)
        {
            ServerLogger::logError("Exception in ParseDocXRoute::handle: %s", e.what());
            json error_response = {
                {"success", false},
                {"error", "Internal server error"},
                {"details", e.what()}};
            sendJsonResponse(sock, error_response, 500);
        }
    }

} // namespace kolosal
