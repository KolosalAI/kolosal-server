#include "kolosal/retrieval/parse_pdf.hpp"

namespace retrieval {

// When PoDoFo is disabled, provide stubs that return a clear error but keep API intact

ParseResult DocumentParser::parse_pdf(const std::string &/*file_path*/,
                                      PDFParseMethod /*method*/,
                                      const std::string &/*language*/,
                                      ProgressCallback /*progress_cb*/)
{
    ParseResult result;
    result.success = false;
    result.error_message = "PDF parsing is not available in this build (PoDoFo disabled).";
    result.pages_processed = 0;
    return result;
}

ParseResult DocumentParser::parse_pdf_from_bytes(const unsigned char * /*data*/, size_t /*size*/,
                                                 PDFParseMethod /*method*/,
                                                 const std::string &/*language*/,
                                                 ProgressCallback /*progress_cb*/)
{
    ParseResult result;
    result.success = false;
    result.error_message = "PDF parsing is not available in this build (PoDoFo disabled).";
    result.pages_processed = 0;
    return result;
}

std::future<ParseResult> DocumentParser::parse_pdf_async(const std::string &file_path,
                                                         PDFParseMethod method,
                                                         const std::string &language,
                                                         ProgressCallback progress_cb)
{
    return std::async(std::launch::async, [=](){
        return parse_pdf(file_path, method, language, progress_cb);
    });
}

std::vector<std::future<ParseResult>> DocumentParser::parse_pdf_batch(
    const std::vector<std::string> &file_paths,
    PDFParseMethod method,
    const std::string &language,
    size_t /*max_concurrent*/)
{
    std::vector<std::future<ParseResult>> futures;
    futures.reserve(file_paths.size());
    for (const auto &path : file_paths) {
        futures.emplace_back(std::async(std::launch::async, [=](){
            return parse_pdf(path, method, language);
        }));
    }
    return futures;
}

bool DocumentParser::is_valid_pdf(const std::string & /*file_path*/)
{
    return false;
}

size_t DocumentParser::get_page_count(const std::string & /*file_path*/)
{
    return 0;
}

// Private helpers (not used in stub builds)
ParseResult DocumentParser::parse_pdf_fast(const std::string & /*file_path*/, ProgressCallback /*progress_cb*/)
{
    return parse_pdf("", PDFParseMethod::Fast, "eng", nullptr);
}

ParseResult DocumentParser::parse_pdf_ocr(const std::string & /*file_path*/, const std::string & /*language*/, ProgressCallback /*progress_cb*/)
{
    return parse_pdf("", PDFParseMethod::OCR, "eng", nullptr);
}

ParseResult DocumentParser::parse_pdf_visual(const std::string & /*file_path*/, ProgressCallback /*progress_cb*/)
{
    return parse_pdf("", PDFParseMethod::Visual, "eng", nullptr);
}

ParseResult DocumentParser::parse_pdf_fast_from_bytes(const unsigned char * /*data*/, size_t /*size*/, ProgressCallback /*progress_cb*/)
{
    return parse_pdf_from_bytes(nullptr, 0, PDFParseMethod::Fast, "eng", nullptr);
}

ParseResult DocumentParser::parse_pdf_ocr_from_bytes(const unsigned char * /*data*/, size_t /*size*/, const std::string & /*language*/, ProgressCallback /*progress_cb*/)
{
    return parse_pdf_from_bytes(nullptr, 0, PDFParseMethod::OCR, "eng", nullptr);
}

ParseResult DocumentParser::parse_pdf_visual_from_bytes(const unsigned char * /*data*/, size_t /*size*/, ProgressCallback /*progress_cb*/)
{
    return parse_pdf_from_bytes(nullptr, 0, PDFParseMethod::Visual, "eng", nullptr);
}

std::string DocumentParser::extract_text_from_page(const PoDoFo::PdfMemDocument& /*doc*/, int /*page_num*/)
{
    return {};
}

bool DocumentParser::file_exists(const std::string & /*file_path*/)
{
    return false;
}

bool DocumentParser::has_pdf_extension(const std::string & /*file_path*/)
{
    return false;
}

} // namespace retrieval
