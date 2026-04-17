#ifndef MF_HEADER_DB_IMPORT_H
#define MF_HEADER_DB_IMPORT_H

#include "mf_core.h"

mf_error_t mf_update_message_analysis_result(
    const char *target_db_path,
    const char *msg_log_id,
    const char *decision,
    int final_score
);

mf_error_t mf_import_header_text_to_db(
    const char *raw_headers,
    const mf_import_options_t *options,
    int serial,
    char **out_msg_log_id
);

mf_error_t mf_create_empty_db_impl(
    const char *target_db_path
);

mf_error_t mf_clone_schema_from_db_impl(
    const char *source_db_path,
    const char *target_db_path
);

#endif
