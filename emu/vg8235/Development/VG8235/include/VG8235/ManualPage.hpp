#pragma once

#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace vg8235 {

namespace manual_detail {

inline std::string escapeHtml(const std::string& text)
{
    std::string result;
    for (const char c : text) {
        switch (c) {
        case '&': result += "&amp;"; break;
        case '<': result += "&lt;"; break;
        case '>': result += "&gt;"; break;
        case '"': result += "&quot;"; break;
        default: result += c; break;
        }
    }
    return result;
}

// Anchor of a heading as GitHub forms it: lower case, letters, digits, '-' and '_', spaces as '-'.
inline std::string headingAnchor(const std::string& heading)
{
    std::string result;
    for (const char c : heading) {
        const auto u = static_cast<unsigned char>(c);
        if (std::isalnum(u) || c == '-' || c == '_') result += static_cast<char>(std::tolower(u));
        else if (c == ' ') result += '-';
    }
    return result;
}

// A relative link points into the folder of the manual; anchors and absolute URLs stay.
inline std::string resolveLink(const std::string& target, const std::string& baseUrl)
{
    if (target.empty() || target[0] == '#' || target.find("://") != std::string::npos ||
        target.rfind("mailto:", 0) == 0) {
        return target;
    }
    return baseUrl + target;
}

inline std::string renderInline(const std::string& text, const std::string& baseUrl)
{
    std::string result;
    std::size_t i = 0;
    while (i < text.size()) {
        const char c = text[i];
        const bool hasNext = i + 1 < text.size();
        if (c == '\\' && hasNext && std::strchr("\\`*_[]()#+-.!|<>", text[i + 1]) != nullptr) {
            result += escapeHtml(std::string(1, text[i + 1]));
            i += 2;
            continue;
        }
        if (c == '`') {
            const auto end = text.find('`', i + 1);
            if (end != std::string::npos) {
                result += "<code>" + escapeHtml(text.substr(i + 1, end - i - 1)) + "</code>";
                i = end + 1;
                continue;
            }
        }
        if (c == '*' && hasNext && text[i + 1] == '*') {
            const auto end = text.find("**", i + 2);
            if (end != std::string::npos) {
                result += "<strong>" + renderInline(text.substr(i + 2, end - i - 2), baseUrl) + "</strong>";
                i = end + 2;
                continue;
            }
        }
        if (c == '*' && hasNext && text[i + 1] != ' ') {
            const auto end = text.find('*', i + 1);
            if (end != std::string::npos && text[end - 1] != ' ') {
                result += "<em>" + renderInline(text.substr(i + 1, end - i - 1), baseUrl) + "</em>";
                i = end + 1;
                continue;
            }
        }
        if (c == '[') {
            const auto close = text.find(']', i + 1);
            if (close != std::string::npos && close + 1 < text.size() && text[close + 1] == '(') {
                const auto end = text.find(')', close + 2);
                if (end != std::string::npos) {
                    const auto target = resolveLink(text.substr(close + 2, end - close - 2), baseUrl);
                    result += "<a href=\"" + escapeHtml(target) + "\">" +
                              renderInline(text.substr(i + 1, close - i - 1), baseUrl) + "</a>";
                    i = end + 1;
                    continue;
                }
            }
        }
        result += escapeHtml(std::string(1, c));
        ++i;
    }
    return result;
}

inline std::string trim(const std::string& text)
{
    const auto first = text.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    const auto last = text.find_last_not_of(" \t");
    return text.substr(first, last - first + 1);
}

// Cells of a table row; '|' inside code spans or after a backslash does not split.
inline std::vector<std::string> tableCells(const std::string& line)
{
    std::vector<std::string> cells;
    std::string cell;
    bool inCode = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '`') inCode = !inCode;
        if (c == '|' && !inCode && (i == 0 || line[i - 1] != '\\')) {
            cells.push_back(trim(cell));
            cell.clear();
        } else {
            cell += c;
        }
    }
    cells.push_back(trim(cell));
    if (!cells.empty() && cells.front().empty()) cells.erase(cells.begin());
    if (!cells.empty() && cells.back().empty()) cells.pop_back();
    return cells;
}

inline bool tableSeparator(const std::string& line)
{
    const auto text = trim(line);
    if (text.empty() || text[0] != '|') return false;
    return text.find_first_not_of("|-: ") == std::string::npos && text.find('-') != std::string::npos;
}

struct ListItem {
    std::size_t indent = 0;
    bool ordered = false;
    std::string number;
    std::string text;
};

inline bool parseListItem(const std::string& line, ListItem& item)
{
    const auto indent = line.find_first_not_of(' ');
    if (indent == std::string::npos) return false;
    item.indent = indent;
    if ((line[indent] == '-' || line[indent] == '*') && indent + 1 < line.size() && line[indent + 1] == ' ') {
        item.ordered = false;
        item.text = line.substr(indent + 2);
        return true;
    }
    auto digits = indent;
    while (digits < line.size() && std::isdigit(static_cast<unsigned char>(line[digits]))) ++digits;
    if (digits > indent && digits + 1 < line.size() && line[digits] == '.' && line[digits + 1] == ' ') {
        item.ordered = true;
        item.number = line.substr(indent, digits - indent);
        item.text = line.substr(digits + 2);
        return true;
    }
    return false;
}

} // namespace manual_detail

// Converts the Markdown of a user manual (headings, paragraphs, lists, tables, code blocks, code
// spans, emphasis and links) into a complete HTML page. Relative links are resolved against
// baseUrl, the URL of the manual's folder ending in '/'.
inline std::string manualToHtml(const std::string& markdown, const std::string& baseUrl)
{
    using namespace manual_detail;

    std::vector<std::string> lines;
    std::string line;
    for (const char c : markdown) {
        if (c == '\n') {
            lines.push_back(line);
            line.clear();
        } else if (c != '\r') {
            line += c;
        }
    }
    if (!line.empty()) lines.push_back(line);

    std::string body;
    std::string title;
    std::vector<std::string> paragraph;
    struct OpenList {
        std::size_t indent;
        bool ordered;
    };
    std::vector<OpenList> lists;

    const auto flushParagraph = [&] {
        if (paragraph.empty()) return;
        std::string text;
        for (const auto& part : paragraph) text += (text.empty() ? "" : " ") + trim(part);
        body += "<p>" + renderInline(text, baseUrl) + "</p>\n";
        paragraph.clear();
    };
    const auto closeLists = [&](std::size_t keepAbove) {
        while (!lists.empty() && (lists.back().indent > keepAbove || keepAbove == std::string::npos)) {
            body += lists.back().ordered ? "</li></ol>\n" : "</li></ul>\n";
            lists.pop_back();
        }
    };

    for (std::size_t index = 0; index < lines.size(); ++index) {
        const auto& current = lines[index];
        const auto text = trim(current);

        if (text.rfind("```", 0) == 0) {
            flushParagraph();
            closeLists(std::string::npos);
            std::string code;
            for (++index; index < lines.size() && trim(lines[index]).rfind("```", 0) != 0; ++index) {
                code += lines[index] + "\n";
            }
            body += "<pre><code>" + escapeHtml(code) + "</code></pre>\n";
            continue;
        }
        if (text.empty()) {
            flushParagraph();
            continue;
        }
        if (text[0] == '#') {
            const auto level = text.find_first_not_of('#');
            if (level != std::string::npos && level <= 6 && text[level] == ' ') {
                flushParagraph();
                closeLists(std::string::npos);
                const auto heading = trim(text.substr(level));
                if (level == 1 && title.empty()) title = heading;
                std::string plain;
                for (const char c : heading) {
                    if (c != '`' && c != '*') plain += c;
                }
                const auto tag = "h" + std::to_string(level);
                body += "<" + tag + " id=\"" + headingAnchor(plain) + "\">" + renderInline(heading, baseUrl) +
                        "</" + tag + ">\n";
                continue;
            }
        }
        if (text[0] == '|' && index + 1 < lines.size() && tableSeparator(lines[index + 1])) {
            flushParagraph();
            closeLists(std::string::npos);
            body += "<table>\n<thead><tr>";
            for (const auto& cell : tableCells(text)) body += "<th>" + renderInline(cell, baseUrl) + "</th>";
            body += "</tr></thead>\n<tbody>\n";
            for (index += 2; index < lines.size() && trim(lines[index]).rfind('|', 0) == 0; ++index) {
                body += "<tr>";
                for (const auto& cell : tableCells(trim(lines[index]))) {
                    body += "<td>" + renderInline(cell, baseUrl) + "</td>";
                }
                body += "</tr>\n";
            }
            body += "</tbody>\n</table>\n";
            --index;
            continue;
        }
        ListItem item;
        if (parseListItem(current, item)) {
            flushParagraph();
            closeLists(item.indent);
            if (!lists.empty() && lists.back().indent == item.indent && lists.back().ordered != item.ordered) {
                body += lists.back().ordered ? "</li></ol>\n" : "</li></ul>\n";
                lists.pop_back();
            }
            if (!lists.empty() && lists.back().indent == item.indent) {
                body += "</li>\n<li>";
            } else {
                if (item.ordered) body += item.number == "1" ? "<ol>\n<li>" : "<ol start=\"" + item.number + "\">\n<li>";
                else body += "<ul>\n<li>";
                lists.push_back({item.indent, item.ordered});
            }
            body += renderInline(item.text, baseUrl);
            continue;
        }
        if (!lists.empty() && current.size() > text.size() && current[0] == ' ') {
            body += " " + renderInline(text, baseUrl);
            continue;
        }
        closeLists(std::string::npos);
        paragraph.push_back(current);
    }
    flushParagraph();
    closeLists(std::string::npos);

    std::string plainTitle;
    for (const char c : title) {
        if (c != '`' && c != '*') plainTitle += c;
    }
    return "<!doctype html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n"
           "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n<title>" +
           escapeHtml(plainTitle) +
           "</title>\n<style>\n"
           ":root { color-scheme: light dark; --text: #1f2328; --background: #ffffff; --muted: #f6f8fa; "
           "--border: #d1d9e0; --link: #0969da; }\n"
           "@media (prefers-color-scheme: dark) { :root { --text: #e6edf3; --background: #0d1117; "
           "--muted: #161b22; --border: #3d444d; --link: #4493f8; } }\n"
           "body { margin: 0 auto; max-width: 980px; padding: 24px 32px; font: 15px/1.55 'Segoe UI', sans-serif; "
           "color: var(--text); background: var(--background); }\n"
           "h1, h2 { border-bottom: 1px solid var(--border); padding-bottom: 0.3em; }\n"
           "h2 { margin-top: 2em; }\n"
           "a { color: var(--link); text-decoration: none; }\n"
           "a:hover { text-decoration: underline; }\n"
           "code { font: 13px Consolas, monospace; background: var(--muted); padding: 0.1em 0.35em; "
           "border-radius: 4px; }\n"
           "pre { background: var(--muted); padding: 12px 16px; border-radius: 6px; overflow-x: auto; }\n"
           "pre code { padding: 0; background: none; }\n"
           "table { border-collapse: collapse; margin: 1em 0; display: block; overflow-x: auto; }\n"
           "th, td { border: 1px solid var(--border); padding: 5px 12px; text-align: left; vertical-align: top; }\n"
           "th { background: var(--muted); }\n"
           "</style>\n</head>\n<body>\n" +
           body + "</body>\n</html>\n";
}

} // namespace vg8235
