#include "mf_header_db_import.h"
#include "mf_header_parse_utils.h"
#include "mf_error.h"

#include <sqlite3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

static char *dup_cstr(const std::string &s) {
    char *p = static_cast<char*>(std::malloc(s.size() + 1));
    if (!p) return nullptr;
    std::memcpy(p, s.c_str(), s.size() + 1);
    return p;
}

static std::string trim_crlf(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) {
        s.pop_back();
    }
    return s;
}


static std::string normalize_subject_simple(const std::string &subject) {
    /* MVP: erstmal nur Rückgabe unverändert.
       Später an bestehende mailfilter-Logik angleichen. */
    return subject;
}

static int exec_sql(sqlite3 *db, const char *sql) {
    char *errmsg = nullptr;
    const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errmsg);
    if (errmsg) sqlite3_free(errmsg);
    return rc;
}

static mf_error_t create_core_schema(sqlite3 *db) {
    const char *sql_messages =
        "CREATE TABLE IF NOT EXISTS messages ("
        " msg_log_id TEXT PRIMARY KEY,"
        " message_id TEXT,"
        " from_addr TEXT,"
        " to_addr TEXT,"
        " subject TEXT,"
        " normal_subject TEXT,"
        " date_hdr TEXT,"
        " msg_size INTEGER,"
        " decision TEXT,"
        " final_score INTEGER DEFAULT 0,"
        " created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";

    const char *sql_header_entries =
        "CREATE TABLE IF NOT EXISTS header_entries ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " msg_log_id TEXT NOT NULL,"
        " ordinal INTEGER NOT NULL,"
        " tag TEXT NOT NULL,"
        " body TEXT NOT NULL,"
        " created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";

    const char *sql_rule_hits =
        "CREATE TABLE IF NOT EXISTS rule_hits ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " msg_log_id TEXT NOT NULL,"
        " phase TEXT NOT NULL,"
        " expression TEXT,"
        " is_negative INTEGER NOT NULL DEFAULT 0,"
        " matched INTEGER NOT NULL DEFAULT 0,"
        " header_tag TEXT,"
        " header_body TEXT,"
        " normalized_subject INTEGER NOT NULL DEFAULT 0,"
        " score_delta INTEGER NOT NULL DEFAULT 0,"
        " created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";

    const char *sql_idx_header_entries_msg_log_id =
        "CREATE INDEX IF NOT EXISTS idx_header_entries_msg_log_id "
        "ON header_entries(msg_log_id);";

    const char *sql_idx_rule_hits_msg_log_id =
        "CREATE INDEX IF NOT EXISTS idx_rule_hits_msg_log_id "
        "ON rule_hits(msg_log_id);";

    const char *sql_idx_rule_hits_phase =
        "CREATE INDEX IF NOT EXISTS idx_rule_hits_phase "
        "ON rule_hits(phase);";

    if (exec_sql(db, sql_messages) != SQLITE_OK) return MF_ERR_DB_SCHEMA;
    if (exec_sql(db, sql_header_entries) != SQLITE_OK) return MF_ERR_DB_SCHEMA;
    if (exec_sql(db, sql_rule_hits) != SQLITE_OK) return MF_ERR_DB_SCHEMA;

    if (exec_sql(db, sql_idx_header_entries_msg_log_id) != SQLITE_OK) return MF_ERR_DB_SCHEMA;
    if (exec_sql(db, sql_idx_rule_hits_msg_log_id) != SQLITE_OK) return MF_ERR_DB_SCHEMA;
    if (exec_sql(db, sql_idx_rule_hits_phase) != SQLITE_OK) return MF_ERR_DB_SCHEMA;

    return MF_OK;
}

} // namespace


mf_error_t mf_create_empty_db_impl(const char *target_db_path) {
    if (!target_db_path || !*target_db_path) {
        return MF_ERR_INVALID_ARG;
    }

    sqlite3 *db = nullptr;
    if (sqlite3_open(target_db_path, &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return MF_ERR_DB_OPEN;
    }

    mf_error_t err = create_core_schema(db);
    sqlite3_close(db);
    return err;
}

mf_error_t mf_clone_schema_from_db_impl(
    const char *source_db_path,
    const char *target_db_path
) {
    /* MVP: später aus sqlite_master kopieren.
       Vorläufig auf leeres Kernschema zurückfallen. */
    (void)source_db_path;
    return mf_create_empty_db_impl(target_db_path);
}

mf_error_t mf_import_header_text_to_db(
    const char *raw_headers,
    const mf_import_options_t *options,
    int serial,
    char **out_msg_log_id
) {
    if (!raw_headers || !options || !options->target_db_path) {
        return MF_ERR_INVALID_ARG;
    }

    if (out_msg_log_id) *out_msg_log_id = nullptr;

    std::vector<std::pair<std::string, std::string>> fields;
    mf_error_t err = mf_parse_headers_to_fields(raw_headers, fields);
    if (err != MF_OK) return err;

    sqlite3 *db = nullptr;
    if (sqlite3_open(options->target_db_path, &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return MF_ERR_DB_OPEN;
    }

    err = create_core_schema(db);
    if (err != MF_OK) {
        sqlite3_close(db);
        return err;
    }

    std::string id_prefix = options->id_prefix ? options->id_prefix : "imp-";
    const int start_serial = (options->start_serial > 0) ? options->start_serial : 1;
    const int serial_value = start_serial + serial - 1;
    std::string msg_log_id = id_prefix + std::to_string(serial_value);

    std::string message_id, from_addr, to_addr, subject, date_hdr;
    for (const auto &f : fields) {
        if (f.first == "Message-ID") message_id = f.second;
        else if (f.first == "From") from_addr = f.second;
        else if (f.first == "To") to_addr = f.second;
        else if (f.first == "Subject") subject = f.second;
        else if (f.first == "Date") date_hdr = f.second;
    }

    std::string normal_subject = normalize_subject_simple(subject);
    const char *decision = options->decision ? options->decision : "imported";
    const int final_score = 0;
    const int msg_size = static_cast<int>(std::strlen(raw_headers));

    if (exec_sql(db, "BEGIN TRANSACTION;") != SQLITE_OK) {
        sqlite3_close(db);
        return MF_ERR_DB_WRITE;
    }

    sqlite3_stmt *stmt_msg = nullptr;
    const char *sql_msg =
        "INSERT INTO MESSAGES "
        "(msg_log_id, message_id, from_addr, to_addr, subject, normal_subject, date_hdr, msg_size, decision, final_score) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql_msg, -1, &stmt_msg, nullptr) != SQLITE_OK) {
        exec_sql(db, "ROLLBACK;");
        sqlite3_close(db);
        return MF_ERR_DB_WRITE;
    }

    sqlite3_bind_text(stmt_msg, 1, msg_log_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt_msg, 2, message_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt_msg, 3, from_addr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt_msg, 4, to_addr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt_msg, 5, subject.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt_msg, 6, normal_subject.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt_msg, 7, date_hdr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt_msg, 8, msg_size);
    sqlite3_bind_text(stmt_msg, 9, decision, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt_msg, 10, final_score);

    if (sqlite3_step(stmt_msg) != SQLITE_DONE) {
        sqlite3_finalize(stmt_msg);
        exec_sql(db, "ROLLBACK;");
        sqlite3_close(db);
        return MF_ERR_DB_WRITE;
    }
    sqlite3_finalize(stmt_msg);

    sqlite3_stmt *stmt_hdr = nullptr;
    const char *sql_hdr =
        "INSERT INTO HEADER_ENTRIES (msg_log_id, ordinal, tag, body) "
        "VALUES (?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql_hdr, -1, &stmt_hdr, nullptr) != SQLITE_OK) {
        exec_sql(db, "ROLLBACK;");
        sqlite3_close(db);
        return MF_ERR_DB_WRITE;
    }

    int ordinal = 1;
    for (const auto &f : fields) {
        sqlite3_reset(stmt_hdr);
        sqlite3_clear_bindings(stmt_hdr);

        sqlite3_bind_text(stmt_hdr, 1, msg_log_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt_hdr, 2, ordinal++);
        sqlite3_bind_text(stmt_hdr, 3, f.first.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt_hdr, 4, f.second.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt_hdr) != SQLITE_DONE) {
            sqlite3_finalize(stmt_hdr);
            exec_sql(db, "ROLLBACK;");
            sqlite3_close(db);
            return MF_ERR_DB_WRITE;
        }
    }
    sqlite3_finalize(stmt_hdr);

    if (exec_sql(db, "COMMIT;") != SQLITE_OK) {
        exec_sql(db, "ROLLBACK;");
        sqlite3_close(db);
        return MF_ERR_DB_WRITE;
    }

    sqlite3_close(db);

    if (out_msg_log_id) {
        *out_msg_log_id = dup_cstr(msg_log_id);
        if (!*out_msg_log_id) return MF_ERR_OOM;
    }

    return MF_OK;
}

mf_error_t mf_update_message_analysis_result(
    const char *target_db_path,
    const char *msg_log_id,
    const char *decision,
    int final_score
) {
    if (!target_db_path || !msg_log_id || !decision) {
        return MF_ERR_INVALID_ARG;
    }

    sqlite3 *db = nullptr;
    if (sqlite3_open(target_db_path, &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return MF_ERR_DB_OPEN;
    }

    sqlite3_stmt *stmt = nullptr;
    const char *sql =
        "UPDATE messages "
        "SET decision = ?, final_score = ? "
        "WHERE msg_log_id = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        return MF_ERR_DB_WRITE;
    }

    sqlite3_bind_text(stmt, 1, decision, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, final_score);
    sqlite3_bind_text(stmt, 3, msg_log_id, -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    if (rc != SQLITE_DONE) {
        return MF_ERR_DB_WRITE;
    }

    return MF_OK;
}
