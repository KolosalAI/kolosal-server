#pragma once

#include "../export.hpp"
#include <future>
#include <mutex>
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <regex>
#include <stdexcept>
#include <cstring>
#include <zip.h>
#include <pugixml.hpp>

namespace retrieval
{
    class DOCXParser
    {
    public:
        // Synchronous parsing
        static std::string parse_docx(const std::string &file_path);

        // Parse from memory buffer
        static std::string parse_docx_from_bytes(const unsigned char *data, size_t size);

        // Asynchronous parsing
        static std::future<std::string> parse_docx_async(const std::string &file_path);

        // Batch processing
        static std::vector<std::future<std::string>> parse_docx_batch(
            const std::vector<std::string> &file_paths);

        // Utility functions
        static bool is_valid_docx(const std::string &file_path);
        static size_t get_page_count(const std::string &file_path);

    private:
        // Thread-safe parsing methods
        static std::string parse_docx_internal(const std::string &file_path);

        // Memory-based parsing methods
        static std::string parse_docx_from_bytes_internal(const unsigned char *data, size_t size);

        // Utility functions
        static bool file_exists(const std::string &file_path);
        static bool has_docx_extension(const std::string &file_path);
    }
};
