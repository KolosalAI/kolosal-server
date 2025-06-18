#include "kolosal/retrieval/parse_pdf.hpp"
#include <mupdf/fitz.h>
#include <stdexcept>
#include <sstream>
#include <filesystem>
#include <thread>
#include <algorithm>
#include <fstream>
#include <mutex>

namespace retrieval
{
    thread_local std::unique_ptr<MuPDFContext> tls_context;

    // RAII MuPDF Context Management
    MuPDFContext::MuPDFContext() : ctx_(nullptr)
    {
        ctx_ = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
        if (ctx_)
        {
            fz_register_document_handlers(ctx_);
        }
    }

    MuPDFContext::~MuPDFContext()
    {
        if (ctx_)
        {
            fz_drop_context(ctx_);
            ctx_ = nullptr;
        }
    }

    MuPDFContext::MuPDFContext(MuPDFContext &&other) noexcept : ctx_(other.ctx_)
    {
        other.ctx_ = nullptr;
    }

    MuPDFContext &MuPDFContext::operator=(MuPDFContext &&other) noexcept
    {
        if (this != &other)
        {
            if (ctx_)
            {
                fz_drop_context(ctx_);
            }
            ctx_ = other.ctx_;
            other.ctx_ = nullptr;
        }
        return *this;
    }    // RAII MuPDF Document Management
    MuPDFDocument::MuPDFDocument(fz_context *ctx, const std::string &filepath)
        : ctx_(ctx), doc_(nullptr), stream_(nullptr)
    {
        if (!ctx_)
        {
            throw std::invalid_argument("Invalid MuPDF context");
        }

        fz_try(ctx_)
        {
            doc_ = fz_open_document(ctx_, filepath.c_str());
        }
        fz_catch(ctx_)
        {
            throw std::runtime_error("Failed to open PDF file: " + filepath);
        }
    }

    MuPDFDocument::MuPDFDocument(fz_context *ctx, const unsigned char *data, size_t size)
        : ctx_(ctx), doc_(nullptr), stream_(nullptr)
    {
        if (!ctx_)
        {
            throw std::invalid_argument("Invalid MuPDF context");
        }

        if (!data || size == 0)
        {
            throw std::invalid_argument("Invalid PDF data");
        }

        fz_try(ctx_)
        {
            stream_ = fz_open_memory(ctx_, data, size);
            doc_ = fz_open_document_with_stream(ctx_, "pdf", stream_);
        }
        fz_catch(ctx_)
        {
            if (stream_)
            {
                fz_drop_stream(ctx_, stream_);
                stream_ = nullptr;
            }
            throw std::runtime_error("Failed to open PDF from memory");
        }
    }    MuPDFDocument::~MuPDFDocument()
    {
        if (doc_ && ctx_)
        {
            fz_drop_document(ctx_, doc_);
            doc_ = nullptr;
        }
        if (stream_ && ctx_)
        {
            fz_drop_stream(ctx_, stream_);
            stream_ = nullptr;
        }
    }    MuPDFDocument::MuPDFDocument(MuPDFDocument &&other) noexcept
        : ctx_(other.ctx_), doc_(other.doc_), stream_(other.stream_)
    {
        other.ctx_ = nullptr;
        other.doc_ = nullptr;
        other.stream_ = nullptr;
    }

    MuPDFDocument &MuPDFDocument::operator=(MuPDFDocument &&other) noexcept
    {
        if (this != &other)
        {
            if (doc_ && ctx_)
            {
                fz_drop_document(ctx_, doc_);
            }
            if (stream_ && ctx_)
            {
                fz_drop_stream(ctx_, stream_);
            }
            ctx_ = other.ctx_;
            doc_ = other.doc_;
            stream_ = other.stream_;
            other.ctx_ = nullptr;
            other.doc_ = nullptr;
            other.stream_ = nullptr;
        }
        return *this;
    }

    int MuPDFDocument::page_count() const
    {
        if (!doc_ || !ctx_)
        {
            return 0;
        }
        return fz_count_pages(ctx_, doc_);
    }

    // Public API Implementation
    ParseResult DocumentParser::parse_pdf(const std::string &file_path,
                                          PDFParseMethod method,
                                          const std::string &language,
                                          ProgressCallback progress_cb)
    {
        try
        {
            // Validate input
            if (!file_exists(file_path))
            {
                return ParseResult("", false, 0);
            }

            if (!has_pdf_extension(file_path))
            {
                ParseResult result;
                result.success = false;
                result.error_message = "File does not have a PDF extension";
                return result;
            }

            // Dispatch to appropriate parsing method
            switch (method)
            {
            case PDFParseMethod::Fast:
                return parse_pdf_fast(file_path, progress_cb);
            case PDFParseMethod::OCR:
                return parse_pdf_ocr(file_path, language, progress_cb);
            case PDFParseMethod::Visual:
                return parse_pdf_visual(file_path, progress_cb);
            default:
                ParseResult result;
                result.success = false;
                result.error_message = "Unsupported PDF parse method";
                return result;
            }
        }
        catch (const std::exception &e)
        {            ParseResult result;
            result.success = false;
            result.error_message = e.what();
            return result;
        }
    }

    ParseResult DocumentParser::parse_pdf_from_bytes(const unsigned char *data, size_t size,
                                                     PDFParseMethod method,
                                                     const std::string &language,
                                                     ProgressCallback progress_cb)
    {
        try
        {
            // Validate input
            if (!data || size == 0)
            {
                ParseResult result;
                result.success = false;
                result.error_message = "Invalid PDF data";
                return result;
            }

            // Dispatch to appropriate parsing method
            switch (method)
            {
            case PDFParseMethod::Fast:
                return parse_pdf_fast_from_bytes(data, size, progress_cb);
            case PDFParseMethod::OCR:
                return parse_pdf_ocr_from_bytes(data, size, language, progress_cb);
            case PDFParseMethod::Visual:
                return parse_pdf_visual_from_bytes(data, size, progress_cb);
            default:
                ParseResult result;
                result.success = false;
                result.error_message = "Unsupported PDF parse method";
                return result;
            }
        }
        catch (const std::exception &e)
        {
            ParseResult result;
            result.success = false;
            result.error_message = e.what();
            return result;
        }
    }

    std::future<ParseResult> DocumentParser::parse_pdf_async(const std::string &file_path,
                                                             PDFParseMethod method,
                                                             const std::string &language,
                                                             ProgressCallback progress_cb)
    {
        return std::async(std::launch::async, [=]()
                          { return parse_pdf(file_path, method, language, progress_cb); });
    }

    std::vector<std::future<ParseResult>> DocumentParser::parse_pdf_batch(
        const std::vector<std::string> &file_paths,
        PDFParseMethod method,
        const std::string &language,
        size_t max_concurrent)
    {
        std::vector<std::future<ParseResult>> futures;
        futures.reserve(file_paths.size());

        // Limit concurrent operations based on hardware
        const size_t hardware_threads = std::thread::hardware_concurrency();
        const size_t actual_max = std::min(max_concurrent,
                                           std::max(static_cast<size_t>(1), hardware_threads));

        for (const auto &file_path : file_paths)
        {
            futures.emplace_back(
                std::async(std::launch::async, [=]()
                           { return parse_pdf(file_path, method, language); }));
        }

        return futures;
    }

    bool DocumentParser::is_valid_pdf(const std::string &file_path)
    {
        if (!file_exists(file_path) || !has_pdf_extension(file_path))
        {
            return false;
        }

        try
        {
            // Get thread-local context
            if (!tls_context)
            {
                tls_context = std::make_unique<MuPDFContext>();
            }

            if (!tls_context->is_valid())
            {
                return false;
            }

            MuPDFDocument doc(tls_context->get(), file_path);
            return doc.is_valid() && doc.page_count() > 0;
        }
        catch (...)
        {
            return false;
        }
    }

    size_t DocumentParser::get_page_count(const std::string &file_path)
    {
        if (!is_valid_pdf(file_path))
        {
            return 0;
        }

        try
        {
            if (!tls_context)
            {
                tls_context = std::make_unique<MuPDFContext>();
            }

            MuPDFDocument doc(tls_context->get(), file_path);
            return static_cast<size_t>(doc.page_count());
        }
        catch (...)
        {
            return 0;
        }
    }

    // Private Implementation Methods
    ParseResult DocumentParser::parse_pdf_fast(const std::string &file_path,
                                               ProgressCallback progress_cb)
    {
        try
        {
            // Get thread-local context for thread safety
            if (!tls_context)
            {
                tls_context = std::make_unique<MuPDFContext>();
            }

            if (!tls_context->is_valid())
            {
                ParseResult result;
                result.success = false;
                result.error_message = "Failed to create MuPDF context";
                return result;
            }

            MuPDFDocument doc(tls_context->get(), file_path);

            const int page_count = doc.page_count();
            if (page_count <= 0)
            {
                ParseResult result;
                result.success = false;
                result.error_message = "PDF has no pages";
                return result;
            }

            std::ostringstream output;
            output.str().reserve(page_count * 1024); // Pre-allocate reasonable space

            for (int i = 0; i < page_count; ++i)
            {
                try
                {
                    std::string page_text = extract_text_from_page(tls_context->get(), doc.get(), i);
                    if (!page_text.empty())
                    {
                        output << page_text;
                        if (i < page_count - 1)
                        {
                            output << "\n\n"; // Page separator
                        }
                    }

                    // Progress callback
                    if (progress_cb)
                    {
                        progress_cb(i + 1, page_count);
                    }
                }
                catch (const std::exception &e)
                {
                    // Log page error but continue processing
                    output << "[Error processing page " << (i + 1) << ": " << e.what() << "]\n\n";
                }
            }

            ParseResult result(output.str(), true, page_count);
            return result;
        }
        catch (const std::exception &e)
        {
            ParseResult result;
            result.success = false;
            result.error_message = e.what();
            return result;
        }
    }

    ParseResult DocumentParser::parse_pdf_ocr(const std::string &file_path,
                                              const std::string &language,
                                              ProgressCallback progress_cb)
    {
        // TODO: Implement OCR using Tesseract
        // This would involve:
        // 1. Converting PDF pages to images using MuPDF
        // 2. Using Tesseract to perform OCR on each image
        // 3. Combining the results

        ParseResult result;
        result.success = false;
        result.error_message = "OCR parsing not yet implemented. Please use Fast method or implement Tesseract integration.";
        return result;
    }

    ParseResult DocumentParser::parse_pdf_visual(const std::string &file_path,                                                 ProgressCallback progress_cb)
    {
        // TODO: Implement visual parsing using ML models
        // This would involve:
        // 1. Converting PDF pages to images
        // 2. Using a vision model (like GPT-4V, PaddleOCR, or custom model) to extract text
        // 3. Combining and structuring the results

        ParseResult result;
        result.success = false;
        result.error_message = "Visual parsing not yet implemented. Please use Fast method or implement vision model integration.";
        return result;
    }

    // Memory-based parsing methods
    ParseResult DocumentParser::parse_pdf_fast_from_bytes(const unsigned char *data, size_t size,
                                                         ProgressCallback progress_cb)
    {
        try
        {
            // Get thread-local context for thread safety
            if (!tls_context)
            {
                tls_context = std::make_unique<MuPDFContext>();
            }

            if (!tls_context->is_valid())
            {
                ParseResult result;
                result.success = false;
                result.error_message = "Failed to create MuPDF context";
                return result;
            }

            MuPDFDocument doc(tls_context->get(), data, size);

            const int page_count = doc.page_count();
            if (page_count <= 0)
            {
                ParseResult result;
                result.success = false;
                result.error_message = "PDF has no pages";
                return result;
            }

            std::ostringstream output;
            output.str().reserve(page_count * 1024); // Pre-allocate reasonable space

            for (int i = 0; i < page_count; ++i)
            {
                try
                {
                    std::string page_text = extract_text_from_page(tls_context->get(), doc.get(), i);
                    if (!page_text.empty())
                    {
                        output << page_text;
                        if (i < page_count - 1)
                        {
                            output << "\n\n"; // Page separator
                        }
                    }

                    // Progress callback
                    if (progress_cb)
                    {
                        progress_cb(i + 1, page_count);
                    }
                }
                catch (const std::exception &e)
                {
                    // Log page error but continue processing
                    output << "[Error processing page " << (i + 1) << ": " << e.what() << "]\n\n";
                }
            }

            ParseResult result(output.str(), true, page_count);
            return result;
        }
        catch (const std::exception &e)
        {
            ParseResult result;
            result.success = false;
            result.error_message = e.what();
            return result;
        }
    }

    ParseResult DocumentParser::parse_pdf_ocr_from_bytes(const unsigned char *data, size_t size,
                                                        const std::string &language,
                                                        ProgressCallback progress_cb)
    {
        // TODO: Implement OCR using Tesseract for memory data
        ParseResult result;
        result.success = false;
        result.error_message = "OCR parsing from bytes not yet implemented. Please use Fast method or implement Tesseract integration.";
        return result;
    }

    ParseResult DocumentParser::parse_pdf_visual_from_bytes(const unsigned char *data, size_t size,
                                                           ProgressCallback progress_cb)
    {
        // TODO: Implement visual parsing using ML models for memory data
        ParseResult result;
        result.success = false;
        result.error_message = "Visual parsing from bytes not yet implemented. Please use Fast method or implement vision model integration.";
        return result;
    }

    // Helper Functions
    std::string DocumentParser::extract_text_from_page(fz_context *ctx, fz_document *doc, int page_num)
    {
        fz_page *page = nullptr;
        fz_stext_page *stext = nullptr;
        fz_device *renderer = nullptr;
        fz_buffer *buf = nullptr;
        std::string result;

        fz_try(ctx)
        {
            page = fz_load_page(ctx, doc, page_num);
            fz_rect bounds = fz_bound_page(ctx, page);

            // 1) Build stext page & device
            stext = fz_new_stext_page(ctx, bounds);
            renderer = fz_new_stext_device(ctx, stext, nullptr);

            // 2) Render page into stext
            fz_run_page(ctx, page, renderer, fz_identity, nullptr);

            // 3) Serialize the stext_page into a buffer
            buf = fz_new_buffer(ctx, 1024);
            fz_output *out = fz_new_output_with_buffer(ctx, buf);
            fz_print_stext_page_as_text(ctx, out, stext);
            fz_drop_output(ctx, out);

            // 4) Copy from buffer into std::string
            size_t len;
            unsigned char *data = nullptr;
            len = fz_buffer_storage(ctx, buf, &data);
            result.assign(reinterpret_cast<char *>(data), len);
        }
        fz_always(ctx)
        {
            if (renderer)
                fz_drop_device(ctx, renderer);
            if (stext)
                fz_drop_stext_page(ctx, stext);
            if (page)
                fz_drop_page(ctx, page);
            if (buf)
                fz_drop_buffer(ctx, buf);
        }
        fz_catch(ctx)
        {
            throw std::runtime_error("Error extracting text from page " + std::to_string(page_num + 1));
        }

        return result;
    }

    bool DocumentParser::file_exists(const std::string &file_path)
    {
        return std::filesystem::exists(file_path) && std::filesystem::is_regular_file(file_path);
    }

    bool DocumentParser::has_pdf_extension(const std::string &file_path)
    {
        std::filesystem::path path(file_path);
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        return extension == ".pdf";
    }

} // namespace retrieval