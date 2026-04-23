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

mf_error_t mf_insert_rule_hit(
    const char *target_db_path,
    const char *msg_log_id,
    const char *phase,
    const char *expression,
    int is_negative,
    int matched,
    const char *header_tag,
    const char *header_body,
    int normalized_subject,
    int score_delta
);

#endif
