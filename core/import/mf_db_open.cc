#include "mf_db_open.h"

#include <sqlite3.h>

namespace {
    sqlite3 *g_external_db = nullptr;

    static int table_exists(sqlite3 *db, const char *name) {
        const char *sql =
            "SELECT 1 FROM sqlite_master "
            "WHERE type='table' AND name=? LIMIT 1";
        sqlite3_stmt *stmt = nullptr;
        int found = 0;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return 0;
        }

        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            found = 1;
        }
        sqlite3_finalize(stmt);
        return found;
    }
}

mf_error_t mf_db_open_existing(const char *db_path, int open_read_only) {
    if (!db_path || !*db_path) {
        return MF_ERR_INVALID_ARG;
    }

    if (g_external_db) {
        sqlite3_close(g_external_db);
        g_external_db = nullptr;
    }

    int flags = open_read_only
        ? SQLITE_OPEN_READONLY
        : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);

    if (sqlite3_open_v2(db_path, &g_external_db, flags, nullptr) != SQLITE_OK) {
        if (g_external_db) {
            sqlite3_close(g_external_db);
            g_external_db = nullptr;
        }
        return MF_ERR_DB_OPEN;
    }

    return mf_db_validate_required_schema();
}

mf_error_t mf_db_close_existing(void) {
    if (g_external_db) {
        sqlite3_close(g_external_db);
        g_external_db = nullptr;
    }
    return MF_OK;
}

mf_error_t mf_db_validate_required_schema(void) {
    if (!g_external_db) {
        return MF_ERR_NOT_INITIALIZED;
    }

    if (!table_exists(g_external_db, "MESSAGES")) {
        return MF_ERR_DB_SCHEMA;
    }
    if (!table_exists(g_external_db, "HEADER_ENTRIES")) {
        return MF_ERR_DB_SCHEMA;
    }
    if (!table_exists(g_external_db, "RULE_HITS")) {
        return MF_ERR_DB_SCHEMA;
    }

    return MF_OK;
}
