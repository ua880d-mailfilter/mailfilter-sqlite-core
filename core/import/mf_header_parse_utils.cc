#include "mf_header_parse_utils.h"

bool mf_is_continuation_line(const std::string &line) {
    return !line.empty() && (line[0] == ' ' || line[0] == '\t');
}

bool mf_split_header_line(
    const std::string &line,
    std::string &tag,
    std::string &body
) {
    const std::size_t pos = line.find(':');
    if (pos == std::string::npos || pos == 0) {
        return false;
    }

    tag = line.substr(0, pos);
    body = (pos + 1 < line.size()) ? line.substr(pos + 1) : "";
    if (!body.empty() && body[0] == ' ') {
        body.erase(0, 1);
    }
    return true;
}

mf_error_t mf_parse_headers_to_fields(
    const char *raw_headers,
    std::vector<std::pair<std::string, std::string>> &fields
) {
    if (!raw_headers) return MF_ERR_INVALID_ARG;

    std::vector<std::string> lines;
    {
        std::string buf;
        for (const char *p = raw_headers; *p; ++p) {
            if (*p == '\r') continue;
            if (*p == '\n') {
                lines.push_back(buf);
                buf.clear();
            } else {
                buf.push_back(*p);
            }
        }
        if (!buf.empty()) lines.push_back(buf);
    }

    std::string current_tag;
    std::string current_body;

    auto flush_current = [&]() {
        if (!current_tag.empty()) {
            fields.emplace_back(current_tag, current_body);
            current_tag.clear();
            current_body.clear();
        }
    };

    for (const std::string &line : lines) {
        if (line.empty()) continue;

        if (mf_is_continuation_line(line)) {
            if (current_tag.empty()) return MF_ERR_FORMAT;
            current_body += "\n";
            current_body += line;
            continue;
        }

        flush_current();

        std::string tag;
        std::string body;
        if (!mf_split_header_line(line, tag, body)) {
            return MF_ERR_FORMAT;
        }

        current_tag = tag;
        current_body = body;
    }

    flush_current();

    return fields.empty() ? MF_ERR_FORMAT : MF_OK;
}
