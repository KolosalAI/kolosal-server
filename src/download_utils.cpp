#include "kolosal/download_utils.hpp"
#include "kolosal/logger.hpp"
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <thread>

namespace kolosal {    // Structure to hold download progress data
    struct DownloadProgressData {
        DownloadProgressCallback callback;
        size_t total_bytes;
        size_t downloaded_bytes;
        volatile bool* cancelled; // Pointer to cancellation flag
        
        DownloadProgressData() : total_bytes(0), downloaded_bytes(0), cancelled(nullptr) {}
    };

    // CURL write callback function
    static size_t write_callback(void* contents, size_t size, size_t nmemb, std::ofstream* file) {
        size_t total_size = size * nmemb;
        file->write(static_cast<const char*>(contents), total_size);
        return total_size;
    }    // CURL progress callback function
    static int curl_progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t) {
        DownloadProgressData* data = static_cast<DownloadProgressData*>(clientp);
        
        // Check if download should be cancelled
        if (data->cancelled && *(data->cancelled)) {
            return 1; // Return non-zero to cancel the download
        }
        
        if (data->callback && dltotal > 0) {
            // For resume downloads, we need to account for already downloaded bytes
            size_t total_bytes = data->total_bytes > 0 ? data->total_bytes : static_cast<size_t>(dltotal);
            size_t current_downloaded = static_cast<size_t>(dlnow);
            
            // If we have a pre-existing download size (resume case), add current progress
            size_t total_downloaded = data->downloaded_bytes + current_downloaded;
            
            // Update the stored values
            data->total_bytes = total_bytes;
            
            // Calculate percentage based on total expected size
            double percentage = total_bytes > 0 ? (static_cast<double>(total_downloaded) / static_cast<double>(total_bytes)) * 100.0 : 0.0;
            
            data->callback(total_downloaded, total_bytes, percentage);
        }
        
        return 0; // Return 0 to continue download
    }

    bool is_valid_url(const std::string& url) {
        // Simple regex to check for HTTP/HTTPS URLs
        std::regex url_regex(R"(^https?:\/\/[^\s\/$.?#].[^\s]*$)", std::regex_constants::icase);
        return std::regex_match(url, url_regex);
    }

    std::string extract_filename_from_url(const std::string& url) {
        // Find the last '/' in the URL
        size_t last_slash = url.find_last_of('/');
        if (last_slash == std::string::npos) {
            return "downloaded_model.gguf"; // Default filename
        }

        std::string filename = url.substr(last_slash + 1);
        
        // Remove query parameters if present
        size_t question_mark = filename.find('?');
        if (question_mark != std::string::npos) {
            filename = filename.substr(0, question_mark);
        }

        // If filename is empty or doesn't have .gguf extension, provide default
        if (filename.empty() || filename.find(".gguf") == std::string::npos) {
            return "downloaded_model.gguf";
        }

        return filename;
    }

    std::string generate_download_path(const std::string& url, const std::string& base_dir) {
        std::string filename = extract_filename_from_url(url);
        
        // Create downloads directory if it doesn't exist
        std::filesystem::create_directories(base_dir);
        
        // Combine base directory and filename
        std::filesystem::path full_path = std::filesystem::path(base_dir) / filename;
        
        return full_path.string();
    }    DownloadResult download_file(const std::string& url, const std::string& local_path, DownloadProgressCallback progress_callback) {
        // Use the resume-enabled function with resume enabled by default
        return download_file_with_resume(url, local_path, progress_callback, true);
    }

    DownloadResult get_url_file_info(const std::string& url) {
        ServerLogger::logInfo("Checking URL accessibility: %s", url.c_str());

        // Validate URL
        if (!is_valid_url(url)) {
            std::string error = "Invalid URL format: " + url;
            ServerLogger::logError("%s", error.c_str());
            return DownloadResult(false, error);
        }

        // Initialize CURL
        CURL* curl = curl_easy_init();
        if (!curl) {
            std::string error = "Failed to initialize CURL";
            ServerLogger::logError("%s", error.c_str());
            return DownloadResult(false, error);
        }

        // Configure CURL for HEAD request
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);  // HEAD request only
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Kolosal-Server/1.0");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);  // 30 second timeout for validation
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        // Perform the HEAD request
        CURLcode res = curl_easy_perform(curl);
        
        // Get response code
        long response_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        // Get content length
        curl_off_t content_length = 0;
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &content_length);

        // Clean up
        curl_easy_cleanup(curl);

        // Check for errors
        if (res != CURLE_OK) {
            std::string error = "URL check failed: " + std::string(curl_easy_strerror(res));
            ServerLogger::logError("%s", error.c_str());
            return DownloadResult(false, error);
        }

        if (response_code != 200) {
            std::string error = "HTTP error: " + std::to_string(response_code);
            ServerLogger::logError("%s", error.c_str());
            return DownloadResult(false, error);
        }

        size_t file_size = (content_length > 0) ? static_cast<size_t>(content_length) : 0;
        ServerLogger::logInfo("URL is accessible. Response code: %ld, Content-Length: %zu bytes", 
                             response_code, file_size);

        return DownloadResult(true, "", "", file_size);
    }    DownloadResult download_file_with_cancellation(const std::string& url, const std::string& local_path, 
                                                  DownloadProgressCallback progress_callback, volatile bool* cancelled) {
        // Use the resume-enabled function with cancellation and resume enabled by default
        return download_file_with_cancellation_and_resume(url, local_path, progress_callback, cancelled, true);
    }

    bool can_resume_download(const std::string& url, const std::string& local_path) {
        // Check if local file exists
        if (!std::filesystem::exists(local_path)) {
            return false;
        }

        try {
            // Get the local file size
            size_t local_size = std::filesystem::file_size(local_path);
            if (local_size == 0) {
                return false; // Empty file, can't resume
            }

            // Get the expected file size from URL
            DownloadResult url_info = get_url_file_info(url);
            if (!url_info.success || url_info.total_bytes == 0) {
                ServerLogger::logWarning("Cannot get file size from URL for resume check: %s", url.c_str());
                return false;
            }

            // Check if local file is smaller than expected (incomplete download)
            if (local_size < url_info.total_bytes) {
                ServerLogger::logInfo("Found partial download: %zu/%zu bytes (%.1f%%) - can resume", 
                                     local_size, url_info.total_bytes, 
                                     (double)local_size / url_info.total_bytes * 100.0);
                return true;
            } else if (local_size == url_info.total_bytes) {
                ServerLogger::logInfo("File already fully downloaded: %zu bytes", local_size);
                return false; // File is complete
            } else {
                ServerLogger::logWarning("Local file is larger than expected - may be corrupted: %zu > %zu bytes", 
                                       local_size, url_info.total_bytes);
                return false; // File is larger than expected, possibly corrupted
            }
        } catch (const std::exception& ex) {
            ServerLogger::logError("Error checking file for resume: %s", ex.what());
            return false;
        }
    }

    DownloadResult download_file_with_resume(const std::string& url, const std::string& local_path, 
                                            DownloadProgressCallback progress_callback, bool resume) {
        ServerLogger::logInfo("Starting download from URL: %s to: %s (resume: %s)", 
                             url.c_str(), local_path.c_str(), resume ? "enabled" : "disabled");

        // Validate URL
        if (!is_valid_url(url)) {
            std::string error = "Invalid URL format: " + url;
            ServerLogger::logError("%s", error.c_str());
            return DownloadResult(false, error);
        }

        // Create directory for the file if it doesn't exist
        std::filesystem::path file_path(local_path);
        std::filesystem::create_directories(file_path.parent_path());

        // Check if we can resume the download
        size_t resume_from = 0;
        size_t expected_total = 0;
        bool resuming = false;

        if (resume && can_resume_download(url, local_path)) {
            resume_from = std::filesystem::file_size(local_path);
            // Get expected file size
            DownloadResult url_info = get_url_file_info(url);
            if (url_info.success) {
                expected_total = url_info.total_bytes;
                resuming = true;
                ServerLogger::logInfo("Resuming download from byte %zu/%zu", resume_from, expected_total);
            }
        } else if (std::filesystem::exists(local_path)) {
            // If file exists but can't resume, remove it and start fresh
            std::filesystem::remove(local_path);
            ServerLogger::logInfo("Existing file cannot be resumed, starting fresh download");
        }

        // Initialize CURL
        CURL* curl = curl_easy_init();
        if (!curl) {
            std::string error = "Failed to initialize CURL";
            ServerLogger::logError("%s", error.c_str());
            return DownloadResult(false, error);
        }

        // Open output file (append mode if resuming)
        std::ofstream output_file(local_path, resuming ? std::ios::binary | std::ios::app : std::ios::binary);
        if (!output_file.is_open()) {
            std::string error = "Failed to create output file: " + local_path;
            ServerLogger::logError("%s", error.c_str());
            curl_easy_cleanup(curl);
            return DownloadResult(false, error);
        }

        // Set up progress data
        DownloadProgressData progress_data;
        progress_data.callback = progress_callback;
        progress_data.total_bytes = expected_total;
        progress_data.downloaded_bytes = resume_from;

        // Configure CURL options
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output_file);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Kolosal-Server/1.0");
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        // Set timeouts to prevent hanging
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);

        // Set resume range if resuming
        if (resuming) {
            std::string range = std::to_string(resume_from) + "-";
            curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());
            ServerLogger::logInfo("Setting HTTP range: %s", range.c_str());
        }

        // Set up progress callback if provided
        if (progress_callback) {
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curl_progress_callback);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_data);
        }

        // Perform the download
        CURLcode res = curl_easy_perform(curl);
        
        // Get response code
        long response_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        // Get content length
        curl_off_t content_length = 0;
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &content_length);

        // Clean up
        output_file.close();
        curl_easy_cleanup(curl);

        // Check for errors
        if (res != CURLE_OK) {
            std::string error = "Download failed: " + std::string(curl_easy_strerror(res));
            ServerLogger::logError("%s", error.c_str());
            
            // Don't remove file if we were resuming - keep partial download
            if (!resuming) {
                std::filesystem::remove(local_path);
            }
            
            return DownloadResult(false, error);
        }

        // For resume, accept both 200 (full file) and 206 (partial content)
        if (response_code != 200 && !(resuming && response_code == 206)) {
            std::string error = "HTTP error: " + std::to_string(response_code);
            ServerLogger::logError("%s", error.c_str());
            
            // Don't remove file if we were resuming - keep partial download
            if (!resuming) {
                std::filesystem::remove(local_path);
            }
            
            return DownloadResult(false, error);
        }

        // Verify file was downloaded and has content
        std::filesystem::path downloaded_file(local_path);
        if (!std::filesystem::exists(downloaded_file) || std::filesystem::file_size(downloaded_file) == 0) {
            std::string error = "Downloaded file is empty or doesn't exist";
            ServerLogger::logError("%s", error.c_str());
            
            // Remove empty file
            if (std::filesystem::exists(downloaded_file)) {
                std::filesystem::remove(downloaded_file);
            }
            
            return DownloadResult(false, error);
        }

        size_t final_size = std::filesystem::file_size(downloaded_file);
        
        // If we know the expected size, verify it matches
        if (expected_total > 0 && final_size != expected_total) {
            ServerLogger::logWarning("Downloaded file size (%zu) doesn't match expected size (%zu)", 
                                   final_size, expected_total);
        }

        ServerLogger::logInfo("Download completed successfully. File size: %zu bytes %s", 
                             final_size, resuming ? "(resumed)" : "(full download)");

        return DownloadResult(true, "", local_path, final_size);
    }

    DownloadResult download_file_with_cancellation_and_resume(const std::string& url, const std::string& local_path, 
                                                             DownloadProgressCallback progress_callback, 
                                                             volatile bool* cancelled, bool resume) {
        ServerLogger::logInfo("Starting download from URL: %s to: %s (resume: %s, cancellation: enabled)", 
                             url.c_str(), local_path.c_str(), resume ? "enabled" : "disabled");

        // Validate URL
        if (!is_valid_url(url)) {
            std::string error = "Invalid URL format: " + url;
            ServerLogger::logError("%s", error.c_str());
            return DownloadResult(false, error);
        }

        // Create directory for the file if it doesn't exist
        std::filesystem::path file_path(local_path);
        std::filesystem::create_directories(file_path.parent_path());

        // Check if we can resume the download
        size_t resume_from = 0;
        size_t expected_total = 0;
        bool resuming = false;

        if (resume && can_resume_download(url, local_path)) {
            resume_from = std::filesystem::file_size(local_path);
            // Get expected file size
            DownloadResult url_info = get_url_file_info(url);
            if (url_info.success) {
                expected_total = url_info.total_bytes;
                resuming = true;
                ServerLogger::logInfo("Resuming download from byte %zu/%zu", resume_from, expected_total);
            }
        } else if (std::filesystem::exists(local_path)) {
            // If file exists but can't resume, remove it and start fresh
            std::filesystem::remove(local_path);
            ServerLogger::logInfo("Existing file cannot be resumed, starting fresh download");
        }

        // Initialize CURL
        CURL* curl = curl_easy_init();
        if (!curl) {
            std::string error = "Failed to initialize CURL";
            ServerLogger::logError("%s", error.c_str());
            return DownloadResult(false, error);
        }

        // Open output file (append mode if resuming)
        std::ofstream output_file(local_path, resuming ? std::ios::binary | std::ios::app : std::ios::binary);
        if (!output_file.is_open()) {
            std::string error = "Failed to create output file: " + local_path;
            ServerLogger::logError("%s", error.c_str());
            curl_easy_cleanup(curl);
            return DownloadResult(false, error);
        }

        // Set up progress data
        DownloadProgressData progress_data;
        progress_data.callback = progress_callback;
        progress_data.total_bytes = expected_total;
        progress_data.downloaded_bytes = resume_from;
        progress_data.cancelled = cancelled;

        // Configure CURL options
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output_file);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Kolosal-Server/1.0");
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        // Set timeouts to prevent hanging
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);

        // Set resume range if resuming
        if (resuming) {
            std::string range = std::to_string(resume_from) + "-";
            curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());
            ServerLogger::logInfo("Setting HTTP range: %s", range.c_str());
        }

        // Set up progress callback if provided
        if (progress_callback || cancelled) {
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curl_progress_callback);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_data);
        }

        // Perform the download
        CURLcode res = curl_easy_perform(curl);
        
        // Get response code
        long response_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        // Get content length
        curl_off_t content_length = 0;
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &content_length);

        // Clean up
        output_file.close();
        curl_easy_cleanup(curl);

        // Check if download was cancelled
        if (cancelled && *cancelled) {
            // Don't remove partially downloaded file - keep for resume
            ServerLogger::logInfo("Download cancelled for URL: %s (partial file preserved for resume)", url.c_str());
            return DownloadResult(false, "Download cancelled by user");
        }

        // Check for errors
        if (res != CURLE_OK) {
            std::string error = "Download failed: " + std::string(curl_easy_strerror(res));
            ServerLogger::logError("%s", error.c_str());
            
            // Don't remove file if we were resuming - keep partial download
            if (!resuming) {
                std::filesystem::remove(local_path);
            }
            
            return DownloadResult(false, error);
        }

        // For resume, accept both 200 (full file) and 206 (partial content)
        if (response_code != 200 && !(resuming && response_code == 206)) {
            std::string error = "HTTP error: " + std::to_string(response_code);
            ServerLogger::logError("%s", error.c_str());
            
            // Don't remove file if we were resuming - keep partial download
            if (!resuming) {
                std::filesystem::remove(local_path);
            }
            
            return DownloadResult(false, error);
        }

        // Verify file was downloaded and has content
        std::filesystem::path downloaded_file(local_path);
        if (!std::filesystem::exists(downloaded_file) || std::filesystem::file_size(downloaded_file) == 0) {
            std::string error = "Downloaded file is empty or doesn't exist";
            ServerLogger::logError("%s", error.c_str());
            
            // Remove empty file
            if (std::filesystem::exists(downloaded_file)) {
                std::filesystem::remove(downloaded_file);
            }
            
            return DownloadResult(false, error);
        }

        size_t final_size = std::filesystem::file_size(downloaded_file);
        
        // If we know the expected size, verify it matches
        if (expected_total > 0 && final_size != expected_total) {
            ServerLogger::logWarning("Downloaded file size (%zu) doesn't match expected size (%zu)", 
                                   final_size, expected_total);
        }

        ServerLogger::logInfo("Download completed successfully. File size: %zu bytes %s", 
                             final_size, resuming ? "(resumed)" : "(full download)");

        return DownloadResult(true, "", local_path, final_size);
    }

} // namespace kolosal
