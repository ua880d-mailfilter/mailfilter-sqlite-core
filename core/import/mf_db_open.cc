#include "mf_db_open.h"

#include <sqlite3.h>
#include <cstdio>
#include <cstring>
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

static void copy_sqlite_text(char *dst, size_t dst_size, const unsigned char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    std::snprintf(dst, dst_size, "%s", reinterpret_cast<const char *>(src));
}

} // Ende namespace

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

mf_error_t mf_db_get_counts(
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

mf_error_t mf_db_get_message_count(int *out_count)
{
    if (!g_external_db) {
        return MF_ERR_NOT_INITIALIZED;
    }

    if (!out_count) {
        return MF_ERR_INVALID_ARG;
    }

    mf_error_t err = mf_db_validate_required_schema();
    if (err != MF_OK) {
        return err;
    }

    return query_table_count(g_external_db, "messages", out_count);
}

mf_error_t mf_db_get_message_summary_at(
    int index,
    mf_message_summary_t *out_summary
)
{
    if (!g_external_db) {
        return MF_ERR_NOT_INITIALIZED;
    }

    if (!out_summary || index < 0) {
        return MF_ERR_INVALID_ARG;
    }

    mf_error_t err = mf_db_validate_required_schema();
    if (err != MF_OK) {
        return err;
    }

    std::memset(out_summary, 0, sizeof(*out_summary));

    const char *sql =
        "SELECT msg_log_id, decision, final_score, subject "
        "FROM messages "
        "ORDER BY msg_log_id "
        "LIMIT 1 OFFSET ?;";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(g_external_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return MF_ERR_DB_OPEN;
    }

    sqlite3_bind_int(stmt, 1, index);

    err = MF_ERR_INVALID_ARG;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        copy_sqlite_text(
            out_summary->msg_log_id,
            sizeof(out_summary->msg_log_id),
            sqlite3_column_text(stmt, 0)
        );
        copy_sqlite_text(
            out_summary->decision,
            sizeof(out_summary->decision),
            sqlite3_column_text(stmt, 1)
        );
        out_summary->final_score = sqlite3_column_int(stmt, 2);
        copy_sqlite_text(
            out_summary->subject,
            sizeof(out_summary->subject),
            sqlite3_column_text(stmt, 3)
        );
        err = MF_OK;
    }

    sqlite3_finalize(stmt);
    return err;
}

mf_error_t mf_db_get_header_count_for_message(
    const char *msg_log_id,
    int *out_count
)
{
    if (!g_external_db) {
        return MF_ERR_NOT_INITIALIZED;
    }

    if (!msg_log_id || !*msg_log_id || !out_count) {
        return MF_ERR_INVALID_ARG;
    }

    mf_error_t err = mf_db_validate_required_schema();
    if (err != MF_OK) {
        return err;
    }

    const char *sql =
        "SELECT COUNT(*) "
        "FROM header_entries "
        "WHERE msg_log_id = ?;";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(g_external_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return MF_ERR_DB_OPEN;
    }

    sqlite3_bind_text(stmt, 1, msg_log_id, -1, SQLITE_STATIC);

    err = MF_ERR_DB_OPEN;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *out_count = sqlite3_column_int(stmt, 0);
        err = MF_OK;
    }

    sqlite3_finalize(stmt);
    return err;
}

mf_error_t mf_db_get_header_at(
    const char *msg_log_id,
    int index,
    mf_header_entry_t *out_entry
)
{
    if (!g_external_db) {
        return MF_ERR_NOT_INITIALIZED;
    }

    if (!msg_log_id || !*msg_log_id || !out_entry || index < 0) {
        return MF_ERR_INVALID_ARG;
    }

    mf_error_t err = mf_db_validate_required_schema();
    if (err != MF_OK) {
        return err;
    }

    std::memset(out_entry, 0, sizeof(*out_entry));

    const char *sql =
        "SELECT msg_log_id, ordinal, tag, body "
        "FROM header_entries "
        "WHERE msg_log_id = ? "
        "ORDER BY ordinal "
        "LIMIT 1 OFFSET ?;";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(g_external_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return MF_ERR_DB_OPEN;
    }

    sqlite3_bind_text(stmt, 1, msg_log_id, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, index);

    err = MF_ERR_INVALID_ARG;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        copy_sqlite_text(
            out_entry->msg_log_id,
            sizeof(out_entry->msg_log_id),
            sqlite3_column_text(stmt, 0)
        );
        out_entry->ordinal = sqlite3_column_int(stmt, 1);
        copy_sqlite_text(
            out_entry->tag,
            sizeof(out_entry->tag),
            sqlite3_column_text(stmt, 2)
        );
        copy_sqlite_text(
            out_entry->body,
            sizeof(out_entry->body),
            sqlite3_column_text(stmt, 3)
        );
        err = MF_OK;
    }

    sqlite3_finalize(stmt);
    return err;
}
