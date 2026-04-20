#ifndef MF_CORE_H
#define MF_CORE_H

#include "mf_error.h"
#include "mf_types.h"

#ifdef __cplusplus
extern "C" {
#endif

mf_error_t mf_init(const mf_config_t *cfg);
int mf_is_initialized(void);
mf_error_t mf_get_version(mf_version_t *out_version);

mf_error_t mf_analyze_header_text(
    const char *raw_headers,
    mf_result_t *out_result
);

mf_error_t mf_analyze_eml_file(
    const char *eml_path,
    mf_result_t *out_result
);

mf_error_t mf_open_existing_db(
    const char *source_db_path,
    int open_read_only
);

mf_error_t mf_close_existing_db(void);
mf_error_t mf_validate_schema(void);

mf_error_t mf_get_db_counts(
    int *out_messages,
    int *out_header_entries,
    int *out_rule_hits
);

mf_error_t mf_import_sequential_header_file(
    const char *input_path,
    int analyze_and_store,
    int *out_imported_count
);

mf_error_t mf_parse_sequential_header_block(
    const char *input_text,
    char **out_raw_headers
);

void mf_free_result(mf_result_t *result);
void mf_shutdown(void);

mf_error_t mf_clone_schema_from_db(
    const char *source_db_path,
    const char *target_db_path
);

mf_error_t mf_create_empty_db(
    const char *target_db_path
);

mf_error_t mf_import_header_file_with_options(
    const char *input_path,
    const mf_import_options_t *options,
    int *out_imported_count
);

mf_error_t mf_import_header_text_with_options(
    const char *input_text,
    const mf_import_options_t *options,
    int *out_imported_count
);

#ifdef __cplusplus
}
#endif

#endif
