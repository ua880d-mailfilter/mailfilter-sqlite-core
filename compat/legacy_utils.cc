#include "legacy_utils.hh"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <memory>
#include <string>

int cmp_no_case(const std::string& a, const std::string& b)
{
    if (a.size() != b.size())
        return 1;

    for (std::size_t i = 0; i < a.size(); ++i) {
        unsigned char ca = static_cast<unsigned char>(a[i]);
        unsigned char cb = static_cast<unsigned char>(b[i]);
        if (std::tolower(ca) != std::tolower(cb))
            return 1;
    }
    return 0;
}

std::string int_to_string(int value)
{
    return std::to_string(value);
}

std::string exec_shell(const char* cmd)
{
    if (!cmd || !*cmd)
        return {};

    std::array<char, 256> buffer{};
    std::string result;

    FILE* pipe = popen(cmd, "r");
    if (!pipe)
        return {};

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result += buffer.data();
    }

    pclose(pipe);
    return result;
}
