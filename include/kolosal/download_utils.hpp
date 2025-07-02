#pragma once

#include "export.hpp"
#include <string>
#include <functional>

namespace kolosal {

    // Progress callback function type
    // Parameters: downloaded_bytes, total_bytes, percentage
    using DownloadProgressCallback = std::function<void(size_t, size_t, double)>;

    // Download result structure
    struct DownloadResult {
        bool success;
        std::string error_message;
        std::string local_path;
        size_t total_bytes;

        DownloadResult() : success(false), total_bytes(0) {}
        DownloadResult(bool success, const std::string& error = "", const std::string& path = "", size_t bytes = 0)
            : success(success), error_message(error), local_path(path), total_bytes(bytes) {}
    };

    /**
     * Check if a string is a valid HTTP/HTTPS URL
     * @param url The URL string to validate
     * @return true if the string is a valid HTTP/HTTPS URL
     */
    KOLOSAL_SERVER_API bool is_valid_url(const std::string& url);

    /**
     * Extract filename from URL
     * @param url The URL to extract filename from
     * @return The filename extracted from the URL
     */
    KOLOSAL_SERVER_API std::string extract_filename_from_url(const std::string& url);

    /**
     * Download a file from a URL to a local path
     * @param url The URL to download from
     * @param local_path The local path to save the file to
     * @param progress_callback Optional callback for progress updates
     * @return DownloadResult containing success status and details
     */
    KOLOSAL_SERVER_API DownloadResult download_file(
        const std::string& url,
        const std::string& local_path,
        DownloadProgressCallback progress_callback = nullptr
    );

    /**
     * Download a file from a URL to a local path with cancellation support
     * @param url The URL to download from
     * @param local_path The local path to save the file to
     * @param progress_callback Optional callback for progress updates
     * @param cancelled Pointer to cancellation flag
     * @return DownloadResult containing success status and details
     */
    KOLOSAL_SERVER_API DownloadResult download_file_with_cancellation(
        const std::string& url,
        const std::string& local_path,
        DownloadProgressCallback progress_callback,
        volatile bool* cancelled
    );

    /**
     * Generate a temporary download path for a URL
     * @param url The URL to generate a path for
     * @param base_dir Base directory for downloads (default: "./models")
     * @return Generated local path for the download
     */
    KOLOSAL_SERVER_API std::string generate_download_path(
        const std::string& url,
        const std::string& base_dir = "./models"
    );

    /**
     * Get file information from a URL without downloading (HEAD request)
     * @param url The URL to check
     * @return DownloadResult with file size and accessibility information
     */
    KOLOSAL_SERVER_API DownloadResult get_url_file_info(const std::string& url);

    /**
     * Check if a partial download exists and can be resumed
     * @param url The URL to check
     * @param local_path The local path where the file should be downloaded
     * @return true if the file can be resumed, false otherwise
     */
    KOLOSAL_SERVER_API bool can_resume_download(const std::string& url, const std::string& local_path);

    /**
     * Download a file from a URL to a local path with resume support
     * @param url The URL to download from
     * @param local_path The local path to save the file to
     * @param progress_callback Optional callback for progress updates
     * @param resume Whether to attempt to resume if partial file exists
     * @return DownloadResult containing success status and details
     */
    KOLOSAL_SERVER_API DownloadResult download_file_with_resume(
        const std::string& url,
        const std::string& local_path,
        DownloadProgressCallback progress_callback = nullptr,
        bool resume = true
    );

    /**
     * Download a file from a URL to a local path with cancellation and resume support
     * @param url The URL to download from
     * @param local_path The local path to save the file to
     * @param progress_callback Optional callback for progress updates
     * @param cancelled Pointer to cancellation flag
     * @param resume Whether to attempt to resume if partial file exists
     * @return DownloadResult containing success status and details
     */
    KOLOSAL_SERVER_API DownloadResult download_file_with_cancellation_and_resume(
        const std::string& url,
        const std::string& local_path,
        DownloadProgressCallback progress_callback,
        volatile bool* cancelled,
        bool resume = true
    );

} // namespace kolosal
