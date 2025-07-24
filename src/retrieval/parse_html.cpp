#include "kolosal/retrieval/parse_html.hpp"
#include <regex>
#include <algorithm>
#include <sstream>
#include <map>
#include <future>
#include <thread>
#include <mutex>

namespace retrieval
{
    HtmlParser::HtmlParser() 
        : is_busy_(std::make_unique<std::atomic<bool>>(false))
        , should_cancel_(std::make_unique<std::atomic<bool>>(false))
    {
    }
    
    HtmlParser::~HtmlParser() = default;

    std::future<HtmlParseResult> HtmlParser::parseHtml(
        const std::string& html_content,
        HtmlProgressCallback progress_callback)
    {
        return std::async(std::launch::async, [this, html_content, progress_callback]() {
            return parseHtmlSync(html_content);
        });
    }

    HtmlParseResult HtmlParser::parseHtmlSync(const std::string& html_content)
    {
        std::lock_guard<std::mutex> lock(busy_mutex_);
        *is_busy_ = true;
        *should_cancel_ = false;

        try
        {
            if (html_content.empty())
            {
                *is_busy_ = false;
                return HtmlParseResult("", false, "Empty HTML content provided", 0);
            }

            std::string markdown = convertHtmlToMarkdown(html_content);
            markdown = cleanupMarkdown(markdown);

            *is_busy_ = false;
            return HtmlParseResult(markdown, true, "", 1);
        }
        catch (const std::exception& e)
        {
            *is_busy_ = false;
            return HtmlParseResult("", false, std::string("HTML parsing error: ") + e.what(), 0);
        }
    }

    bool HtmlParser::isBusy() const
    {
        return is_busy_->load();
    }

    void HtmlParser::cancel()
    {
        *should_cancel_ = true;
    }

    std::string HtmlParser::decodeHtmlEntities(const std::string& html)
    {
        std::string result = html;
        
        // Common HTML entities
        std::map<std::string, std::string> entities = {
            {"&amp;", "&"},
            {"&lt;", "<"},
            {"&gt;", ">"},
            {"&quot;", "\""},
            {"&apos;", "'"},
            {"&nbsp;", " "},
            {"&hellip;", "..."},
            {"&mdash;", "-"},
            {"&ndash;", "-"},
            {"&lsquo;", "'"},
            {"&rsquo;", "'"},
            {"&ldquo;", "\""},
            {"&rdquo;", "\""},
            {"&copy;", "(c)"},
            {"&reg;", "(r)"},
            {"&trade;", "(tm)"}
        };

        for (const auto& entity : entities)
        {
            size_t pos = 0;
            while ((pos = result.find(entity.first, pos)) != std::string::npos)
            {
                result.replace(pos, entity.first.length(), entity.second);
                pos += entity.second.length();
            }
        }

        // Decode numeric entities (&#123; and &#x1A;) - use simpler approach
        std::regex numeric_entity_regex("&#(\\d+);");
        std::sregex_iterator iter(result.begin(), result.end(), numeric_entity_regex);
        std::sregex_iterator end;
        
        std::string decoded_result;
        size_t last_pos = 0;
        
        for (std::sregex_iterator i = iter; i != end; ++i) {
            std::smatch match = *i;
            decoded_result += result.substr(last_pos, match.position() - last_pos);
            
            int code = std::stoi(match[1].str());
            if (code < 128) {
                decoded_result += static_cast<char>(code);
            } else {
                decoded_result += match.str(); // Keep as is if not ASCII
            }
            
            last_pos = match.position() + match.length();
        }
        
        decoded_result += result.substr(last_pos);
        result = decoded_result;

        return result;
    }

    std::string HtmlParser::convertHtmlToMarkdown(const std::string& html)
    {
        std::string markdown = html;
        
        // Remove HTML comments
        std::regex comment_regex(R"(<!--.*?-->)", std::regex_constants::ECMAScript);
        markdown = std::regex_replace(markdown, comment_regex, "");
        
        // Remove script and style tags completely
        std::regex script_regex(R"(<script[^>]*>.*?</script>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, script_regex, "");
        
        std::regex style_regex(R"(<style[^>]*>.*?</style>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, style_regex, "");
        
        // Convert headings (h1-h6)
        std::regex h1_regex(R"(<h1[^>]*>(.*?)</h1>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, h1_regex, "# $1\n\n");
        
        std::regex h2_regex(R"(<h2[^>]*>(.*?)</h2>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, h2_regex, "## $1\n\n");
        
        std::regex h3_regex(R"(<h3[^>]*>(.*?)</h3>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, h3_regex, "### $1\n\n");
        
        std::regex h4_regex(R"(<h4[^>]*>(.*?)</h4>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, h4_regex, "#### $1\n\n");
        
        std::regex h5_regex(R"(<h5[^>]*>(.*?)</h5>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, h5_regex, "##### $1\n\n");
        
        std::regex h6_regex(R"(<h6[^>]*>(.*?)</h6>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, h6_regex, "###### $1\n\n");
        
        // Convert bold text (strong, b)
        std::regex strong_regex(R"(<(strong|b)[^>]*>(.*?)</\1>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, strong_regex, "**$2**");
        
        // Convert italic text (em, i)
        std::regex em_regex(R"(<(em|i)[^>]*>(.*?)</\1>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, em_regex, "*$2*");
        
        // Convert underline to emphasis (since Markdown doesn't have underline)
        std::regex u_regex(R"(<u[^>]*>(.*?)</u>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, u_regex, "_$1_");
        
        // Convert strikethrough
        std::regex strike_regex(R"(<(s|strike|del)[^>]*>(.*?)</\1>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, strike_regex, "~~$2~~");
        
        // Convert inline code
        std::regex code_regex(R"(<code[^>]*>(.*?)</code>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, code_regex, "`$1`");
        
        // Convert code blocks (pre with code)
        std::regex pre_code_regex(R"(<pre[^>]*>\s*<code[^>]*>(.*?)</code>\s*</pre>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, pre_code_regex, "\n```\n$1\n```\n\n");
        
        // Convert simple pre blocks
        std::regex pre_regex(R"(<pre[^>]*>(.*?)</pre>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, pre_regex, "\n```\n$1\n```\n\n");
        
        // Convert links
        std::regex link_regex(R"(<a[^>]*href\s*=\s*[\"']([^\"']*)[\"'][^>]*>(.*?)</a>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, link_regex, "[$2]($1)");
        
        // Convert images
        std::regex img_regex(R"(<img[^>]*src\s*=\s*[\"']([^\"']*)[\"'][^>]*alt\s*=\s*[\"']([^\"']*)[\"'][^>]*\/?>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, img_regex, "![$2]($1)");
        
        std::regex img_regex2(R"(<img[^>]*alt\s*=\s*[\"']([^\"']*)[\"'][^>]*src\s*=\s*[\"']([^\"']*)[\"'][^>]*\/?>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, img_regex2, "![$1]($2)");
        
        // Convert simple img tags without alt
        std::regex img_simple_regex(R"(<img[^>]*src\s*=\s*[\"']([^\"']*)[\"'][^>]*\/?>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, img_simple_regex, "![]($1)");
        
        // Convert blockquotes
        std::regex blockquote_regex(R"(<blockquote[^>]*>(.*?)</blockquote>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, blockquote_regex, "\n> $1\n\n");
        
        // Convert horizontal rules
        std::regex hr_regex(R"(<hr[^>]*\/?>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, hr_regex, "\n---\n\n");
        
        // Convert line breaks
        std::regex br_regex(R"(<br[^>]*\/?>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, br_regex, "  \n");
        
        // Convert unordered lists
        std::regex ul_start_regex(R"(<ul[^>]*>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, ul_start_regex, "\n");
        
        std::regex ul_end_regex(R"(</ul>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, ul_end_regex, "\n");
        
        // Convert ordered lists
        std::regex ol_start_regex(R"(<ol[^>]*>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, ol_start_regex, "\n");
        
        std::regex ol_end_regex(R"(</ol>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, ol_end_regex, "\n");
        
        // Convert list items (this is a simplified approach)
        std::regex li_regex(R"(<li[^>]*>(.*?)</li>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, li_regex, "- $1\n");
        
        // Convert paragraphs
        std::regex p_regex(R"(<p[^>]*>(.*?)</p>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, p_regex, "$1\n\n");
        
        // Convert divs (simple conversion)
        std::regex div_regex(R"(<div[^>]*>(.*?)</div>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, div_regex, "$1\n");
        
        // Convert spans (just extract content)
        std::regex span_regex(R"(<span[^>]*>(.*?)</span>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        markdown = std::regex_replace(markdown, span_regex, "$1");
        
        // Convert tables (basic support)
        std::regex table_regex(R"(<table[^>]*>(.*?)</table>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        std::regex tr_regex(R"(<tr[^>]*>(.*?)</tr>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        std::regex td_regex(R"(<t[hd][^>]*>(.*?)</t[hd]>)", std::regex_constants::ECMAScript | std::regex_constants::icase);
        
        // Simple table conversion (this is very basic)
        markdown = std::regex_replace(markdown, td_regex, "| $1 ");
        markdown = std::regex_replace(markdown, tr_regex, "$1|\n");
        markdown = std::regex_replace(markdown, table_regex, "\n$1\n");
        
        // Remove any remaining HTML tags
        std::regex html_tag_regex(R"(<[^>]*>)", std::regex_constants::ECMAScript);
        markdown = std::regex_replace(markdown, html_tag_regex, "");
        
        // Decode HTML entities
        markdown = decodeHtmlEntities(markdown);
        
        return markdown;
    }

    std::string HtmlParser::cleanupMarkdown(const std::string& markdown)
    {
        std::string result = markdown;
        
        // Clean up excessive whitespace
        std::regex multiple_spaces_regex(R"( {3,})");
        result = std::regex_replace(result, multiple_spaces_regex, " ");
        
        // Clean up multiple newlines (keep maximum of 2)
        std::regex multiple_newlines_regex(R"(\n{3,})");
        result = std::regex_replace(result, multiple_newlines_regex, "\n\n");
        
        // Remove leading/trailing whitespace from each line
        std::istringstream iss(result);
        std::ostringstream oss;
        std::string line;
        
        while (std::getline(iss, line))
        {
            // Trim leading and trailing whitespace
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t") + 1);
            oss << line << "\n";
        }
        
        result = oss.str();
        
        // Final cleanup: remove trailing newlines
        while (!result.empty() && result.back() == '\n')
        {
            result.pop_back();
        }
        
        return result;
    }

} // namespace retrieval
