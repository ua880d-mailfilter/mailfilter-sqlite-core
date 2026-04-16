#ifndef MF_SEQ_HEADER_IMPORT_H
#define MF_SEQ_HEADER_IMPORT_H

#include "mf_error.h"

mf_error_t mf_parse_sequential_header_block_text(
    const char *input_text,
    char **out_raw_headers
);

mf_error_t mf_import_sequential_header_file_impl(
    const char *input_path,
    int analyze_and_store,
    int *out_imported_count
);

#endif
