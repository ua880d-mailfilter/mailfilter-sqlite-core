#include "mf_paths.h"

#include <sys/stat.h>

int mf_path_exists(const char *path) {
    if (!path || !*path) {
        return 0;
    }
    struct stat st;
    return stat(path, &st) == 0 ? 1 : 0;
}
