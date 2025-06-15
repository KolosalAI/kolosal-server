#pragma once

#include "../export.hpp"
#include <string>

namespace retrieval
{
    enum class PDFParseMethod
    {
        Fast,
        OCR,
        Visual
    };

    class DocumentParser
    {
    public:
        static std::string parse_pdf(const std::string &file_path,
                                     PDFParseMethod method = PDFParseMethod::Fast,
                                     const std::string &language = "eng");

    private:
        // Different parsing methods for PDF files
        static std::string parse_pdf_fast(const std::string &file_path);

        static std::string parse_pdf_ocr(const std::string &file_path,
                                         const std::string &language);

        static std::string parse_pdf_visual(const std::string &file_path);
    };
} // namespace retrieval