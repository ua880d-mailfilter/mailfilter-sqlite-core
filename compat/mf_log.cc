#include "mf_log.h"

#include <cstdio>

void mf_log_info(const char *msg) {
    std::fprintf(stderr, "[mf][info] %s\n", msg ? msg : "");
}

void mf_log_error(const char *msg) {
    std::fprintf(stderr, "[mf][error] %s\n", msg ? msg : "");
}
