#ifndef MF_DB_OPEN_H
#define MF_DB_OPEN_H

#include "mf_error.h"
#include "mf_types.h"

mf_error_t mf_db_open_existing(const char *db_path, int open_read_only);
mf_error_t mf_db_close_existing(void);
mf_error_t mf_db_validate_required_schema(void);

// new DB count
mf_error_t mf_db_get_counts(
	int *out_messages,
	int *out_header_entries,
	int *out_rule_hits
);
// ende DB count

// Beginn DB Read

mf_error_t mf_db_get_message_count(int *out_count);

mf_error_t mf_db_get_message_summary_at(
    int index,
    mf_message_summary_t *out_summary
);

mf_error_t mf_db_get_message_summary_by_id(
    const char *msg_log_id,
    mf_message_summary_t *out_summary
);

mf_error_t mf_db_get_header_count_for_message(
    const char *msg_log_id,
    int *out_count
);

mf_error_t mf_db_get_header_at(
    const char *msg_log_id,
    int index,
    mf_header_entry_t *out_entry
);

mf_error_t mf_db_get_rule_hit_count_for_message(
    const char *msg_log_id,
    int *out_count
);

mf_error_t mf_db_get_rule_hit_at(
    const char *msg_log_id,
    int index,
    mf_rule_hit_t *out_hit
);

mf_error_t mf_db_get_rule_hit_expression_agg_count(int *out_count);

mf_error_t mf_db_get_rule_hit_expression_agg_at(
    int index,
    mf_rule_hit_agg_t *out_agg
);

mf_error_t mf_db_get_rule_hit_expression_tag_agg_count(int *out_count);

mf_error_t mf_db_get_rule_hit_expression_tag_agg_at(
    int index,
    mf_rule_hit_tag_agg_t *out_agg
);

mf_error_t mf_db_get_header_tag_agg_count(int *out_count);

mf_error_t mf_db_get_header_tag_agg_at(
    int index,
    mf_header_tag_agg_t *out_agg
);

// Ende DB Read

#endif
