#pragma once

#include "../export.hpp"
#include <mupdf/fitz.h>
#include <string>
#include <future>
#include <memory>
#include <functional>
#include <vector>

namespace retrieval
{
    enum class PDFParseMethod
    {
        Fast,
        OCR,
        Visual
    };

    struct ParseResult
    {
        std::string text;
        bool success;
        std::string error_message;
        size_t pages_processed;

        ParseResult() : success(false), pages_processed(0) {}
        ParseResult(const std::string &t, bool s = true, size_t pages = 0)
            : text(t), success(s), pages_processed(pages) {}
    };

    using ProgressCallback = std::function<void(size_t current_page, size_t total_pages)>;

    // Helper classes for RAII resource management
    class MuPDFContext
    {
    public:
        MuPDFContext();
        ~MuPDFContext();

        // Non-copyable, movable
        MuPDFContext(const MuPDFContext &) = delete;
        MuPDFContext &operator=(const MuPDFContext &) = delete;
        MuPDFContext(MuPDFContext &&other) noexcept;
        MuPDFContext &operator=(MuPDFContext &&other) noexcept;

        fz_context *get() const { return ctx_; }
        bool is_valid() const { return ctx_ != nullptr; }

    private:
        fz_context *ctx_;
    };

    class MuPDFDocument
    {
    public:
        MuPDFDocument(fz_context *ctx, const std::string &filepath);
        ~MuPDFDocument();

        // Non-copyable, movable
        MuPDFDocument(const MuPDFDocument &) = delete;
        MuPDFDocument &operator=(const MuPDFDocument &) = delete;
        MuPDFDocument(MuPDFDocument &&other) noexcept;
        MuPDFDocument &operator=(MuPDFDocument &&other) noexcept;

        fz_document *get() const { return doc_; }
        bool is_valid() const { return doc_ != nullptr; }
        int page_count() const;

    private:
        fz_context *ctx_;
        fz_document *doc_;
    };

    class DocumentParser
    {
    public:
        // Synchronous parsing
        static ParseResult parse_pdf(const std::string &file_path,
                                     PDFParseMethod method = PDFParseMethod::Fast,
                                     const std::string &language = "eng",
                                     ProgressCallback progress_cb = nullptr);

        // Asynchronous parsing
        static std::future<ParseResult> parse_pdf_async(const std::string &file_path,
                                                        PDFParseMethod method = PDFParseMethod::Fast,
                                                        const std::string &language = "eng",
                                                        ProgressCallback progress_cb = nullptr);

        // Batch processing
        static std::vector<std::future<ParseResult>> parse_pdf_batch(
            const std::vector<std::string> &file_paths,
            PDFParseMethod method = PDFParseMethod::Fast,
            const std::string &language = "eng",
            size_t max_concurrent = 4);

        // Utility functions
        static bool is_valid_pdf(const std::string &file_path);
        static size_t get_page_count(const std::string &file_path);

    private:
        // Thread-safe parsing methods
        static ParseResult parse_pdf_fast(const std::string &file_path,
                                          ProgressCallback progress_cb = nullptr);

        static ParseResult parse_pdf_ocr(const std::string &file_path,
                                         const std::string &language,
                                         ProgressCallback progress_cb = nullptr);

        static ParseResult parse_pdf_visual(const std::string &file_path,
                                            ProgressCallback progress_cb = nullptr);

        // Utility functions
        static std::string extract_text_from_page(fz_context *ctx, fz_document *doc, int page_num);
        static bool file_exists(const std::string &file_path);
        static bool has_pdf_extension(const std::string &file_path);
    };
} // namespace retrieval