#ifndef MF_DB_OPEN_H
#define MF_DB_OPEN_H

#include "mf_error.h"

mf_error_t mf_db_open_existing(const char *db_path, int open_read_only);
mf_error_t mf_db_close_existing(void);
mf_error_t mf_db_validate_required_schema(void);

#endif
