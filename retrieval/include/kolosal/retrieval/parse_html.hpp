#pragma once

#include "../export.hpp"
#include <string>
#include <future>
#include <memory>
#include <functional>
#include <vector>
#include <mutex>
#include <atomic>

namespace retrieval
{
    struct HtmlParseResult
    {
        std::string markdown;
        bool success;
        std::string error_message;
        size_t elements_processed;

        HtmlParseResult() : success(false), elements_processed(0) {}
        HtmlParseResult(const std::string &md, bool s = true, size_t elements = 0)
            : markdown(md), success(s), elements_processed(elements) {}
        HtmlParseResult(const std::string &md, bool s, const std::string &err, size_t elements)
            : markdown(md), success(s), error_message(err), elements_processed(elements) {}
    };

    using HtmlProgressCallback = std::function<void(size_t current_element, size_t total_elements)>;

    class KOLOSAL_SERVER_API HtmlParser
    {
    public:
        HtmlParser();
        ~HtmlParser();

        /**
         * Parse HTML content and convert to Markdown
         * @param html_content The HTML content to parse
         * @param progress_callback Optional callback for progress updates
         * @return Future containing the parse result
         */
        std::future<HtmlParseResult> parseHtml(
            const std::string& html_content,
            HtmlProgressCallback progress_callback = nullptr
        );

        /**
         * Parse HTML content synchronously
         * @param html_content The HTML content to parse
         * @return Parse result
         */
        HtmlParseResult parseHtmlSync(const std::string& html_content);

        /**
         * Check if the parser is currently busy
         * @return true if parsing is in progress
         */
        bool isBusy() const;

        /**
         * Cancel any ongoing parsing operation
         */
        void cancel();

    private:
        std::string convertHtmlToMarkdown(const std::string& html);
        std::string cleanupMarkdown(const std::string& markdown);
        std::string decodeHtmlEntities(const std::string& html);
        
        mutable std::mutex busy_mutex_;
        // Use pointers to avoid DLL export issues with std::atomic
        std::unique_ptr<std::atomic<bool>> is_busy_;
        std::unique_ptr<std::atomic<bool>> should_cancel_;
    };

} // namespace retrieval
