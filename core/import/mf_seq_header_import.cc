#include "mf_seq_header_import.h"
#include "mf_core.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
    static char *dup_string(const std::string &s) {
        char *p = static_cast<char*>(std::malloc(s.size() + 1));
        if (!p) return nullptr;
        std::memcpy(p, s.c_str(), s.size() + 1);
        return p;
    }

    static bool is_continuation_line(const std::string &line) {
        return !line.empty() && (line[0] == ' ' || line[0] == '\t');
    }

    static bool looks_like_header_start(const std::string &line) {
        if (line.empty()) return false;
        if (line == "+OK") return false;
        if (line == ".") return false;

        const std::size_t pos = line.find(':');
        if (pos == std::string::npos || pos == 0) {
            return false;
        }

        for (std::size_t i = 0; i < pos; ++i) {
            unsigned char ch = static_cast<unsigned char>(line[i]);
            if (std::iscntrl(ch)) return false;
        }

        return true;
    }
}

mf_error_t mf_parse_sequential_header_block_text(
    const char *input_text,
    char **out_raw_headers
) {
    if (!input_text || !out_raw_headers) {
        return MF_ERR_INVALID_ARG;
    }

    *out_raw_headers = nullptr;

    std::string input(input_text);
    std::string output;
    std::vector<std::string> lines;
    {
        std::string buf;
        for (char ch : input) {
            if (ch == '\r') continue;
            if (ch == '\n') {
                lines.push_back(buf);
                buf.clear();
            } else {
                buf.push_back(ch);
            }
        }
        if (!buf.empty()) {
            lines.push_back(buf);
        }
    }

    bool saw_any_header = false;

    for (const std::string &line : lines) {
        if (line == "+OK" && !saw_any_header && output.empty()) {
            continue;
        }

        if (line == ".") {
            break;
        }

        if (line.empty()) {
            continue;
        }

        if (is_continuation_line(line)) {
            if (!saw_any_header) {
                return MF_ERR_FORMAT;
            }
            output.append(line);
            output.push_back('\n');
            continue;
        }

        if (!looks_like_header_start(line)) {
            return MF_ERR_FORMAT;
        }

        saw_any_header = true;
        output.append(line);
        output.push_back('\n');
    }

    if (!saw_any_header || output.empty()) {
        return MF_ERR_FORMAT;
    }

    *out_raw_headers = dup_string(output);
    if (!*out_raw_headers) {
        return MF_ERR_OOM;
    }

    return MF_OK;
}

mf_error_t mf_import_sequential_header_file_impl(
    const char *input_path,
    int analyze_and_store,
    int *out_imported_count
) {
    if (!input_path) {
        return MF_ERR_INVALID_ARG;
    }

    if (out_imported_count) {
        *out_imported_count = 0;
    }

    FILE *fp = std::fopen(input_path, "rb");
    if (!fp) {
        return MF_ERR_IO;
    }

    std::vector<std::string> block_lines;
    char buffer[8192];
    int imported = 0;

    auto flush_block = [&](void) -> mf_error_t {
        if (block_lines.empty()) {
            return MF_OK;
        }

        std::string block;
        for (const std::string &line : block_lines) {
            block.append(line);
            block.push_back('\n');
        }

        char *raw_headers = nullptr;
        mf_error_t err = mf_parse_sequential_header_block_text(block.c_str(), &raw_headers);
        if (err != MF_OK) {
            block_lines.clear();
            return err;
        }

        if (analyze_and_store) {
            mf_result_t result{};
            err = mf_analyze_header_text(raw_headers, &result);
            std::free(raw_headers);
            mf_free_result(&result);
            if (err != MF_OK) {
                block_lines.clear();
                return err;
            }
        } else {
            std::free(raw_headers);
        }

        ++imported;
        block_lines.clear();
        return MF_OK;
    };

    while (std::fgets(buffer, sizeof(buffer), fp) != nullptr) {
        std::string line(buffer);

        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }

        if (line.empty() && block_lines.empty()) {
            continue;
        }

        block_lines.push_back(line);

        if (line == ".") {
            mf_error_t err = flush_block();
            if (err != MF_OK) {
                std::fclose(fp);
                return err;
            }
        }
    }

    if (!block_lines.empty()) {
        mf_error_t err = flush_block();
        if (err != MF_OK) {
            std::fclose(fp);
            return err;
        }
    }

    std::fclose(fp);

    if (out_imported_count) {
        *out_imported_count = imported;
    }

    return MF_OK;
}
