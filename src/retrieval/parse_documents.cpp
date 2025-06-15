#pragma once

#include "kolosal/retrieval/parse_documents.hpp"
#include <mupdf/fitz.h>
#include <stdexcept>
#include <sstream>

namespace retrieval
{
    std::string PDFParser::parse_pdf(const std::string &filepath, PDFParseMethod method, const std::string &language)
    {
        switch (method)
        {
        case PDFParseMethod::Fast:
            return parse_pdf_fast(filepath);
        case PDFParseMethod::OCR:
            return parse_pdf_ocr(filepath, language);
        case PDFParseMethod::Visual:
            return parse_pdf_visual(filepath);
        default:
            throw std::invalid_argument("Unsupported PDF parse method.");
        }
    }

    std::string PDFParser::parse_pdf_fast(const std::string &filepath)
    {
        fz_context *ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
        if (!ctx)
            throw std::runtime_error("Failed to create MuPDF context");

        fz_register_document_handlers(ctx);

        fz_document *doc = nullptr;
        fz_try(ctx)
        {
            doc = fz_open_document(ctx, filepath.c_str());
        }
        fz_catch(ctx)
        {
            fz_drop_context(ctx);
            throw std::runtime_error("Failed to open PDF file: " + filepath);
        }

        int page_count = fz_count_pages(ctx, doc);
        std::ostringstream output;

        for (int i = 0; i < page_count; ++i)
        {
            fz_page *page = nullptr;
            fz_text_page *text = nullptr;
            fz_try(ctx)
            {
                page = fz_load_page(ctx, doc, i);
                text = fz_new_text_page(ctx);
                fz_device *dev = fz_new_text_device(ctx, text, nullptr);
                fz_run_page(ctx, page, dev, fz_identity, nullptr);
                fz_drop_device(ctx, dev);
                char *text_str = fz_copy_selection(ctx, text, nullptr);
                output << text_str << "\n";
                fz_free(ctx, text_str);
            }
            fz_always(ctx)
            {
                fz_drop_page(ctx, page);
                fz_drop_text_page(ctx, text);
            }
            fz_catch(ctx)
            {
                fz_drop_document(ctx, doc);
                fz_drop_context(ctx);
                throw std::runtime_error("Error processing page " + std::to_string(i));
            }
        }

        fz_drop_document(ctx, doc);
        fz_drop_context(ctx);
        return output.str();
    }

    std::string PDFParser::parse_pdf_ocr(const std::string &filepath, const std::string &language){
        // Placeholder for OCR implementation
        // TODO: Implement OCR library Tesseract
        return ""}

    std::string PDFParser::parse_pdf_visual(const std::string &filepath){
        // Placeholder for visual parsing implementation
        // Should call a visual model to extract text from images
        return ""}
} // namespace retrieval