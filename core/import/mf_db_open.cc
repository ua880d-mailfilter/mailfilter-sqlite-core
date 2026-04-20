#include "mf_db_open.h"

#include <sqlite3.h>
#include <string>

namespace {
sqlite3 *g_external_db = nullptr;

static int table_exists(sqlite3 *db, const char *name)
{
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

static mf_error_t query_table_count(sqlite3 *db, const char *table_name, int *out_count)
{
    if (!db || !table_name || !out_count) {
        return MF_ERR_INVALID_ARG;
    }

    std::string sql = "SELECT COUNT(*) FROM ";
    sql += table_name;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return MF_ERR_DB_OPEN;
    }

    mf_error_t err = MF_ERR_DB_OPEN;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *out_count = sqlite3_column_int(stmt, 0);
        err = MF_OK;
    }

    sqlite3_finalize(stmt);
    return err;
}
} // namespace

mf_error_t mf_db_open_existing(const char *db_path, int open_read_only)
{
    if (!db_path || !*db_path) {
        return MF_ERR_INVALID_ARG;
    }

    if (g_external_db) {
        sqlite3_close(g_external_db);
        g_external_db = nullptr;
    }

    const int flags = open_read_only
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

mf_error_t mf_db_close_existing(void)
{
    if (g_external_db) {
        sqlite3_close(g_external_db);
        g_external_db = nullptr;
    }

    return MF_OK;
}

mf_error_t mf_db_validate_required_schema(void)
{
    if (!g_external_db) {
        return MF_ERR_NOT_INITIALIZED;
    }

    if (!table_exists(g_external_db, "messages")) {
        return MF_ERR_DB_SCHEMA;
    }

    if (!table_exists(g_external_db, "header_entries")) {
        return MF_ERR_DB_SCHEMA;
    }

    if (!table_exists(g_external_db, "rule_hits")) {
        return MF_ERR_DB_SCHEMA;
    }

    return MF_OK;
}

mf_error_t mf_get_db_counts(
    int *out_messages,
    int *out_header_entries,
    int *out_rule_hits
)
{
    if (!g_external_db) {
        return MF_ERR_NOT_INITIALIZED;
    }

    if (!out_messages || !out_header_entries || !out_rule_hits) {
        return MF_ERR_INVALID_ARG;
    }

    mf_error_t err = mf_db_validate_required_schema();
    if (err != MF_OK) {
        return err;
    }

    err = query_table_count(g_external_db, "messages", out_messages);
    if (err != MF_OK) {
        return err;
    }

    err = query_table_count(g_external_db, "header_entries", out_header_entries);
    if (err != MF_OK) {
        return err;
    }

    err = query_table_count(g_external_db, "rule_hits", out_rule_hits);
    if (err != MF_OK) {
        return err;
    }

    return MF_OK;
}
