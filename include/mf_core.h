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

// New -> DB Read Funktionen

mf_error_t mf_get_message_count(int *out_count);

mf_error_t mf_get_message_summary_at(
    int index,
    mf_message_summary_t *out_summary
);

mf_error_t mf_get_message_summary_by_id(
    const char *msg_log_id,
    mf_message_summary_t *out_summary
);

mf_error_t mf_get_header_count_for_message(
    const char *msg_log_id,
    int *out_count
);

mf_error_t mf_get_header_at(
    const char *msg_log_id,
    int index,
    mf_header_entry_t *out_entry
);

mf_error_t mf_get_rule_hit_count_for_message(
    const char *msg_log_id,
    int *out_count
);

mf_error_t mf_get_rule_hit_at(
    const char *msg_log_id,
    int index,
    mf_rule_hit_t *out_hit
);

// End DB Read

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
