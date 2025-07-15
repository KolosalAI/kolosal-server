#include "kolosal/image_utils.hpp"
#include "kolosal/logger.hpp"
#include <curl/curl.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <thread>
#include <regex>

namespace kolosal {

namespace {
    // Base64 character set
    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    static inline bool is_base64(unsigned char c) {
        return (isalnum(c) || (c == '+') || (c == '/'));
    }

    // CURL callback for writing downloaded data
    struct DownloadData {
        std::vector<unsigned char> data;
    };

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, DownloadData* userData) {
        size_t realsize = size * nmemb;
        unsigned char* data = static_cast<unsigned char*>(contents);
        userData->data.insert(userData->data.end(), data, data + realsize);
        return realsize;
    }
}

std::pair<std::vector<unsigned char>, std::string> ImageUtils::decodeBase64Image(const std::string& dataUrl) {
    // Check if it's a data URL
    if (!isDataUrl(dataUrl)) {
        throw std::runtime_error("Not a valid data URL");
    }
    
    // Extract format and base64 data
    std::string format = extractFormatFromDataUrl(dataUrl);
    
    // Find the base64 data part (after the comma)
    size_t commaPos = dataUrl.find(',');
    if (commaPos == std::string::npos) {
        throw std::runtime_error("Invalid data URL format");
    }
    
    std::string base64Data = dataUrl.substr(commaPos + 1);
    std::vector<unsigned char> decodedData = base64Decode(base64Data);
    
    return {decodedData, format};
}

std::future<std::pair<std::vector<unsigned char>, std::string>> ImageUtils::downloadImageAsync(const std::string& url) {
    return std::async(std::launch::async, [url]() {
        return downloadImageSync(url);
    });
}

std::pair<std::vector<unsigned char>, std::string> ImageUtils::loadImageFromFile(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        throw std::runtime_error("Image file does not exist: " + filePath);
    }
    
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open image file: " + filePath);
    }
    
    // Read file into vector
    std::vector<unsigned char> data(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );
    
    std::string format = extractFormatFromPath(filePath);
    return {data, format};
}

bool ImageUtils::isDataUrl(const std::string& url) {
    return url.length() > 5 && url.substr(0, 5) == "data:";
}

bool ImageUtils::isHttpUrl(const std::string& url) {
    return (url.length() > 7 && url.substr(0, 7) == "http://") ||
           (url.length() > 8 && url.substr(0, 8) == "https://");
}

std::string ImageUtils::extractFormatFromDataUrl(const std::string& dataUrl) {
    // Extract MIME type from data URL
    // Format: data:image/jpeg;base64,... or data:image/png;base64,...
    std::regex mimeRegex(R"(data:image/([^;]+);)");
    std::smatch match;
    
    if (std::regex_search(dataUrl, match, mimeRegex)) {
        return match[1].str();
    }
    
    return "unknown";
}

std::string ImageUtils::extractFormatFromPath(const std::string& filePath) {
    std::string extension = std::filesystem::path(filePath).extension().string();
    if (extension.empty()) {
        return "unknown";
    }
    
    // Remove the dot and convert to lowercase
    extension = extension.substr(1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    // Normalize common formats
    if (extension == "jpg") extension = "jpeg";
    
    return extension;
}

bool ImageUtils::isSupportedFormat(const std::string& format) {
    static const std::vector<std::string> supportedFormats = {
        "jpeg", "jpg", "png", "gif", "bmp", "webp"
    };
    
    std::string lowerFormat = format;
    std::transform(lowerFormat.begin(), lowerFormat.end(), lowerFormat.begin(), ::tolower);
    
    return std::find(supportedFormats.begin(), supportedFormats.end(), lowerFormat) != supportedFormats.end();
}

std::vector<unsigned char> ImageUtils::base64Decode(const std::string& encoded) {
    int in_len = encoded.size();
    int i = 0;
    int in = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::vector<unsigned char> ret;

    while (in_len-- && (encoded[in] != '=') && is_base64(encoded[in])) {
        char_array_4[i++] = encoded[in]; in++;
        if (i == 4) {
            for (i = 0; i < 4; i++) {
                char_array_4[i] = base64_chars.find(char_array_4[i]);
            }

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++) {
                ret.push_back(char_array_3[i]);
            }
            i = 0;
        }
    }

    if (i) {
        for (int j = i; j < 4; j++) {
            char_array_4[j] = 0;
        }

        for (int j = 0; j < 4; j++) {
            char_array_4[j] = base64_chars.find(char_array_4[j]);
        }

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (int j = 0; (j < i - 1); j++) {
            ret.push_back(char_array_3[j]);
        }
    }

    return ret;
}

std::pair<std::vector<unsigned char>, std::string> ImageUtils::downloadImageSync(const std::string& url) {
    CURL* curl;
    CURLcode res;
    DownloadData downloadData;
    
    curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }
    
    // Set CURL options
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &downloadData);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Kolosal-Server/1.0");
    
    // Perform the request
    res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        throw std::runtime_error("Failed to download image: " + std::string(curl_easy_strerror(res)));
    }
    
    // Check HTTP response code
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    
    curl_easy_cleanup(curl);
    
    if (httpCode != 200) {
        throw std::runtime_error("HTTP error " + std::to_string(httpCode) + " when downloading image");
    }
    
    if (downloadData.data.empty()) {
        throw std::runtime_error("Downloaded image is empty");
    }
    
    // Try to determine format from URL extension
    std::string format = extractFormatFromPath(url);
    if (format == "unknown") {
        // Try to detect format from content
        if (downloadData.data.size() >= 4) {
            // Check for JPEG magic bytes
            if (downloadData.data[0] == 0xFF && downloadData.data[1] == 0xD8) {
                format = "jpeg";
            }
            // Check for PNG magic bytes
            else if (downloadData.data[0] == 0x89 && downloadData.data[1] == 0x50 && 
                     downloadData.data[2] == 0x4E && downloadData.data[3] == 0x47) {
                format = "png";
            }
            // Check for GIF magic bytes
            else if (downloadData.data[0] == 0x47 && downloadData.data[1] == 0x49 && 
                     downloadData.data[2] == 0x46) {
                format = "gif";
            }
            // Check for BMP magic bytes
            else if (downloadData.data[0] == 0x42 && downloadData.data[1] == 0x4D) {
                format = "bmp";
            }
            // Check for WebP magic bytes
            else if (downloadData.data.size() >= 12 && 
                     downloadData.data[0] == 0x52 && downloadData.data[1] == 0x49 && 
                     downloadData.data[2] == 0x46 && downloadData.data[3] == 0x46 &&
                     downloadData.data[8] == 0x57 && downloadData.data[9] == 0x45 && 
                     downloadData.data[10] == 0x42 && downloadData.data[11] == 0x50) {
                format = "webp";
            }
        }
    }
    
    return {downloadData.data, format};
}

} // namespace kolosal
