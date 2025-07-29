#include "kolosal/retrieval/parse_pdf.hpp"
#include <podofo/podofo.h>
#include <stdexcept>
#include <sstream>
#include <filesystem>
#include <thread>
#include <algorithm>
#include <fstream>
#include <mutex>

using namespace PoDoFo;

namespace retrieval
{
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
            PdfMemDocument doc;
            doc.Load(file_path);
            auto& pages = doc.GetPages();
            return pages.GetCount() > 0;
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
            PdfMemDocument doc;
            doc.Load(file_path);
            auto& pages = doc.GetPages();
            return static_cast<size_t>(pages.GetCount());
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
            PdfMemDocument doc;
            doc.Load(file_path);
            auto& pages = doc.GetPages();

            const int page_count = static_cast<int>(pages.GetCount());
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
                    std::string page_text = extract_text_from_page(doc, i);
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
        // 1. Converting PDF pages to images using PoDoFo
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
            PdfMemDocument doc;
            doc.LoadFromBuffer({reinterpret_cast<const char*>(data), size});
            auto& pages = doc.GetPages();

            const int page_count = static_cast<int>(pages.GetCount());
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
                    std::string page_text = extract_text_from_page(doc, i);
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
    std::string DocumentParser::extract_text_from_page(const PdfMemDocument& doc, int page_num)
    {
        try
        {
            auto& pages = doc.GetPages();
            if (page_num < 0 || page_num >= static_cast<int>(pages.GetCount()))
            {
                throw std::runtime_error("Page index out of range");
            }

            auto& page = pages.GetPageAt(static_cast<unsigned>(page_num));
            
            std::vector<PdfTextEntry> entries;
            page.ExtractTextTo(entries);

            std::ostringstream output;
            for (const auto& entry : entries)
            {
                output << entry.Text << " ";
            }

            return output.str();
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error("Error extracting text from page " + std::to_string(page_num + 1) + ": " + e.what());
        }
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