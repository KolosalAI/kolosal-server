#include "kolosal/retrieval/parse_docx.hpp"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <thread>
#include <future>
#include <mutex>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <unzip.h>
#include <pugixml.hpp>

namespace retrieval
{
    // Thread safety mutex for minizip operations
    static std::mutex zip_mutex;

    // Forward declaration
    static std::string extract_document_xml_from_file(const std::string &file_path);    // Internal function to extract document.xml from file (without mutex locking)
    static std::string extract_document_xml_from_file_internal(const std::string &file_path)
    {

        unzFile archive = unzOpen(file_path.c_str());
        if (!archive)
        {
            throw std::runtime_error("Failed to open DOCX file: " + file_path);
        }

        // Find document.xml
        if (unzLocateFile(archive, "word/document.xml", 0) != UNZ_OK)
        {
            unzClose(archive);
            throw std::runtime_error("document.xml not found in DOCX file");
        }

        if (unzOpenCurrentFile(archive) != UNZ_OK)
        {
            unzClose(archive);
            throw std::runtime_error("Failed to open document.xml");
        }

        // Read file content
        std::string content;
        char buffer[8192];
        int bytes_read;

        while ((bytes_read = unzReadCurrentFile(archive, buffer, sizeof(buffer))) > 0)
        {
            content.append(buffer, bytes_read);
        }

        unzCloseCurrentFile(archive);
        unzClose(archive);

        if (bytes_read < 0)
        {
            throw std::runtime_error("Error reading document.xml");
        }

        return content;
    }

    // Extract document.xml from file (with mutex locking)
    static std::string extract_document_xml_from_file(const std::string &file_path)
    {
        std::lock_guard<std::mutex> lock(zip_mutex);
        return extract_document_xml_from_file_internal(file_path);
    }

    // Extract document.xml from DOCX archive in memory
    static std::string extract_document_xml_from_memory(const unsigned char *data, size_t size)
    {
        std::lock_guard<std::mutex> lock(zip_mutex);
        
        // Write data to temporary file for minizip
        std::string temp_file = (std::filesystem::temp_directory_path() / "temp_docx.docx").string();
        std::ofstream temp_out(temp_file, std::ios::binary);
        if (!temp_out.is_open())
        {
            throw std::runtime_error("Failed to create temporary file");
        }
        temp_out.write(reinterpret_cast<const char*>(data), size);
        temp_out.close();

        try
        {
            // Use the internal function to avoid double locking
            std::string result = extract_document_xml_from_file_internal(temp_file);
            std::filesystem::remove(temp_file);
            return result;
        }
        catch (...)
        {
            std::filesystem::remove(temp_file);
            throw;
        }
    }

    // Process text run with formatting
    static std::string process_run(const pugi::xml_node &run)
    {
        std::string text;
        bool is_bold = false;
        bool is_italic = false;

        // Check formatting properties
        pugi::xml_node props = run.select_node(".//w:rPr").node();
        if (props)
        {
            is_bold = props.select_node(".//w:b").node() != nullptr;
            is_italic = props.select_node(".//w:i").node() != nullptr;
        }        // Extract text content
        for (pugi::xpath_node xpath_node : run.select_nodes(".//w:t"))
        {
            pugi::xml_node text_node = xpath_node.node();
            text += text_node.text().get();
        }

        // Handle line breaks
        for (pugi::xpath_node xpath_node : run.select_nodes(".//w:br"))
        {
            text += "\n";
        }

        // Apply markdown formatting
        if (is_bold && is_italic)
        {
            text = "***" + text + "***";
        }
        else if (is_bold)
        {
            text = "**" + text + "**";
        }
        else if (is_italic)
        {
            text = "*" + text + "*";
        }

        return text;
    }

    // Process paragraph
    static std::string process_paragraph(const pugi::xml_node &para)
    {
        std::stringstream result;
        bool is_heading = false;
        int heading_level = 1;

        // Check if paragraph is a heading
        pugi::xml_node style = para.select_node(".//w:pStyle").node();
        if (style)
        {
            std::string style_val = style.attribute("w:val").value();
            if (style_val.find("Heading") != std::string::npos ||
                style_val.find("heading") != std::string::npos)
            {
                is_heading = true;

                // Extract heading level
                std::regex heading_regex(R"([Hh]eading\s*(\d+))");
                std::smatch match;
                if (std::regex_search(style_val, match, heading_regex))
                {
                    heading_level = std::stoi(match[1].str());
                }
            }
        }        // Process all text runs in the paragraph
        std::string text_content;
        for (pugi::xpath_node xpath_node : para.select_nodes(".//w:r"))
        {
            pugi::xml_node run = xpath_node.node();
            text_content += process_run(run);
        }

        // Clean up whitespace
        text_content = std::regex_replace(text_content, std::regex(R"(\s+)"), " ");
        text_content = std::regex_replace(text_content, std::regex(R"(^\s+|\s+$)"), "");

        if (text_content.empty())
        {
            return "";
        }

        // Format as heading or regular paragraph
        if (is_heading && heading_level > 0 && heading_level <= 6)
        {
            result << std::string(heading_level, '#') << " " << text_content;
        }
        else
        {
            result << text_content;
        }

        return result.str();
    }

    // Convert XML content to Markdown
    static std::string xml_to_markdown(const std::string &xml_content)
    {
        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_string(xml_content.c_str());

        if (!result)
        {
            throw std::runtime_error("Failed to parse XML: " + std::string(result.description()));
        }

        std::stringstream markdown;

        // Find document body
        pugi::xml_node body = doc.select_node("//w:body").node();
        if (!body)
        {
            throw std::runtime_error("Document body not found in XML");
        }        // Process all paragraphs
        for (pugi::xpath_node xpath_node : body.select_nodes("//w:p"))
        {
            pugi::xml_node para = xpath_node.node();
            std::string paragraph_text = process_paragraph(para);
            if (!paragraph_text.empty())
            {
                markdown << paragraph_text << "\n\n";
            }
        }

        return markdown.str();
    }

    // Public method implementations

    std::string DOCXParser::parse_docx(const std::string &file_path)
    {
        return parse_docx_internal(file_path);
    }

    std::string DOCXParser::parse_docx_from_bytes(const unsigned char *data, size_t size)
    {
        return parse_docx_from_bytes_internal(data, size);
    }

    std::future<std::string> DOCXParser::parse_docx_async(const std::string &file_path)
    {
        return std::async(std::launch::async, [file_path]()
                          { return parse_docx_internal(file_path); });
    }

    std::vector<std::future<std::string>> DOCXParser::parse_docx_batch(
        const std::vector<std::string> &file_paths)
    {

        std::vector<std::future<std::string>> futures;
        futures.reserve(file_paths.size());

        for (const auto &file_path : file_paths)
        {
            futures.push_back(std::async(std::launch::async, [file_path]()
                                         { return parse_docx_internal(file_path); }));
        }

        return futures;
    }

    bool DOCXParser::is_valid_docx(const std::string &file_path)
    {
        if (!file_exists(file_path) || !has_docx_extension(file_path))
        {
            return false;
        }        try
        {
            std::lock_guard<std::mutex> lock(zip_mutex);

            unzFile archive = unzOpen(file_path.c_str());
            if (!archive)
            {
                return false;
            }

            // Check for required DOCX structure
            bool has_document_xml = unzLocateFile(archive, "word/document.xml", 0) == UNZ_OK;
            bool has_content_types = unzLocateFile(archive, "[Content_Types].xml", 0) == UNZ_OK;
            bool has_rels = unzLocateFile(archive, "_rels/.rels", 0) == UNZ_OK;

            unzClose(archive);

            return has_document_xml && has_content_types && has_rels;
        }
        catch (...)
        {
            return false;
        }
    }    size_t DOCXParser::get_page_count(const std::string &file_path)
    {
        if (!file_exists(file_path) || !has_docx_extension(file_path))
        {
            return 0;
        }

        try
        {
            std::lock_guard<std::mutex> lock(zip_mutex);
            
            // Check if it's a valid DOCX (inline check to avoid double locking)
            unzFile archive = unzOpen(file_path.c_str());
            if (!archive)
            {
                return 0;
            }

            // Check for required DOCX structure
            bool has_document_xml = unzLocateFile(archive, "word/document.xml", 0) == UNZ_OK;
            bool has_content_types = unzLocateFile(archive, "[Content_Types].xml", 0) == UNZ_OK;
            bool has_rels = unzLocateFile(archive, "_rels/.rels", 0) == UNZ_OK;

            if (!has_document_xml || !has_content_types || !has_rels)
            {
                unzClose(archive);
                return 0;
            }

            // Extract document.xml content inline to avoid double locking
            std::string xml_content = extract_document_xml_from_file_internal(file_path);

            pugi::xml_document doc;
            if (!doc.load_string(xml_content.c_str()))
            {
                return 0;
            }

            // Count paragraphs as a rough estimate of pages
            // In a real implementation, you might want to parse document properties
            // or use more sophisticated page counting logic
            pugi::xml_node body = doc.select_node("//w:body").node();
            if (!body)
            {
                return 0;
            }            size_t paragraph_count = 0;
            for (pugi::xpath_node xpath_node : body.select_nodes("//w:p"))
            {
                pugi::xml_node para = xpath_node.node();
                if (!process_paragraph(para).empty())
                {
                    paragraph_count++;
                }
            }

            // Rough estimate: assume 10-15 paragraphs per page
            return std::max(static_cast<size_t>(1), (paragraph_count + 12) / 13);
        }
        catch (...)
        {
            return 0;
        }
    }

    // Private method implementations

    std::string DOCXParser::parse_docx_internal(const std::string &file_path)
    {
        if (!file_exists(file_path))
        {
            throw std::runtime_error("File does not exist: " + file_path);
        }

        if (!has_docx_extension(file_path))
        {
            throw std::runtime_error("File is not a DOCX file: " + file_path);
        }

        try
        {
            std::string xml_content = extract_document_xml_from_file(file_path);
            return xml_to_markdown(xml_content);
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error("Failed to parse DOCX file '" + file_path + "': " + e.what());
        }
    }

    std::string DOCXParser::parse_docx_from_bytes_internal(const unsigned char *data, size_t size)
    {
        if (!data || size == 0)
        {
            throw std::invalid_argument("Invalid input data: null pointer or zero size");
        }

        try
        {
            std::string xml_content = extract_document_xml_from_memory(data, size);
            return xml_to_markdown(xml_content);
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error("Failed to parse DOCX from bytes: " + std::string(e.what()));
        }
    }

    bool DOCXParser::file_exists(const std::string &file_path)
    {
        try
        {
            return std::filesystem::exists(file_path) && std::filesystem::is_regular_file(file_path);
        }
        catch (...)
        {
            return false;
        }
    }

    bool DOCXParser::has_docx_extension(const std::string &file_path)
    {
        if (file_path.length() < 5)
        {
            return false;
        }

        std::string extension = file_path.substr(file_path.length() - 5);
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

        return extension == ".docx";
    }

} // namespace retrieval