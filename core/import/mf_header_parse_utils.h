#ifndef MF_HEADER_PARSE_UTILS_H
#define MF_HEADER_PARSE_UTILS_H

#include "mf_error.h"

#include <string>
#include <utility>
#include <vector>

bool mf_is_continuation_line(const std::string &line);

bool mf_split_header_line(
    const std::string &line,
    std::string &tag,
    std::string &body
);

mf_error_t mf_parse_headers_to_fields(
    const char *raw_headers,
    std::vector<std::pair<std::string, std::string>> &fields
);

#endif
