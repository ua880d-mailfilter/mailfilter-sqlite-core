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

// Ende DB Read

#endif
