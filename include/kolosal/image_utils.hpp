#pragma once

#include "export.hpp"
#include <string>
#include <vector>
#include <memory>
#include <future>

namespace kolosal {

/**
 * @brief Utility functions for processing images in chat messages
 */
class KOLOSAL_SERVER_API ImageUtils {
public:
    /**
     * @brief Decode base64 image data from a data URL
     * @param dataUrl The data URL (e.g., "data:image/jpeg;base64,...")
     * @return Pair of (decoded_data, format) where format is "jpeg", "png", etc.
     */
    static std::pair<std::vector<unsigned char>, std::string> decodeBase64Image(const std::string& dataUrl);
    
    /**
     * @brief Download image from HTTP(S) URL
     * @param url The HTTP(S) URL to download from
     * @return Future containing pair of (image_data, format)
     */
    static std::future<std::pair<std::vector<unsigned char>, std::string>> downloadImageAsync(const std::string& url);
    
    /**
     * @brief Load image from local file path
     * @param filePath Path to local image file
     * @return Pair of (image_data, format)
     */
    static std::pair<std::vector<unsigned char>, std::string> loadImageFromFile(const std::string& filePath);
    
    /**
     * @brief Check if URL is a data URL (base64 encoded)
     * @param url The URL to check
     * @return true if URL starts with "data:"
     */
    static bool isDataUrl(const std::string& url);
    
    /**
     * @brief Check if URL is a HTTP(S) URL
     * @param url The URL to check
     * @return true if URL starts with "http://" or "https://"
     */
    static bool isHttpUrl(const std::string& url);
    
    /**
     * @brief Extract format from data URL
     * @param dataUrl The data URL
     * @return Image format (e.g., "jpeg", "png")
     */
    static std::string extractFormatFromDataUrl(const std::string& dataUrl);
    
    /**
     * @brief Extract format from file extension
     * @param filePath The file path
     * @return Image format (e.g., "jpeg", "png")
     */
    static std::string extractFormatFromPath(const std::string& filePath);
    
    /**
     * @brief Validate image format is supported
     * @param format The image format
     * @return true if format is supported
     */
    static bool isSupportedFormat(const std::string& format);

private:
    /**
     * @brief Base64 decode implementation
     * @param encoded The base64 encoded string
     * @return Decoded binary data
     */
    static std::vector<unsigned char> base64Decode(const std::string& encoded);
    
    /**
     * @brief Download image synchronously (used by async version)
     * @param url The URL to download from
     * @return Pair of (image_data, format)
     */
    static std::pair<std::vector<unsigned char>, std::string> downloadImageSync(const std::string& url);
};

} // namespace kolosal
