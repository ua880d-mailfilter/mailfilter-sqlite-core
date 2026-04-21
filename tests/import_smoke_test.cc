#include "mf_core.h"
#include "mf_types.h"

#include <sqlite3.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

static int query_single_int(sqlite3 *db, const char *sql, int *out_value) {
    if (!db || !sql || !out_value) {
        return 0;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    int ok = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *out_value = sqlite3_column_int(stmt, 0);
        ok = 1;
    }

    sqlite3_finalize(stmt);
    return ok;
}

static int query_single_text(sqlite3 *db, const char *sql, std::string &out_value) {
    if (!db || !sql) {
        return 0;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    int ok = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *txt = sqlite3_column_text(stmt, 0);
        out_value = txt ? reinterpret_cast<const char *>(txt) : "";
        ok = 1;
    }

    sqlite3_finalize(stmt);
    return ok;
}

struct MessageRow {
    std::string msg_log_id;
    std::string decision;
    int final_score;
};

static int query_message_rows(sqlite3 *db, std::vector<MessageRow> &rows) {
    if (!db) {
        return 0;
    }

    sqlite3_stmt *stmt = nullptr;
    const char *sql =
        "SELECT msg_log_id, decision, final_score "
        "FROM messages ORDER BY msg_log_id;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    rows.clear();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MessageRow row;
        const unsigned char *msg_log_id = sqlite3_column_text(stmt, 0);
        const unsigned char *decision = sqlite3_column_text(stmt, 1);

        row.msg_log_id = msg_log_id ? reinterpret_cast<const char *>(msg_log_id) : "";
        row.decision = decision ? reinterpret_cast<const char *>(decision) : "";
        row.final_score = sqlite3_column_int(stmt, 2);

        rows.push_back(row);
    }

    sqlite3_finalize(stmt);
    return 1;
}

static int run_import_check(
    const char *scenario_name,
    const char *input_file,
    const char *import_db,
    int analyze_after_import,
    const char *rc_path
) {
    const char *runtime_db = "build/test-runtime.sqlite3";

    std::remove(runtime_db);
    std::remove(import_db);

    mf_shutdown();

    mf_config_t cfg{};
    cfg.db_path = runtime_db;
    cfg.rc_path = rc_path;
    cfg.policy_dir = ".";
    cfg.enable_sqlite_logging = 1;
    cfg.enable_rule_hits = 0;
    cfg.enable_explanations = 0;
    cfg.create_additive_tables = 0;
    cfg.app_id = "smoke-test";
    cfg.device_id = "ci";

    mf_error_t init_err = mf_init(&cfg);
    if (init_err != MF_OK) {
        std::cerr << "mf_init failed for scenario " << scenario_name
                  << " rc_path=" << rc_path
                  << ": " << mf_error_string(init_err) << "\n";
        return 2;
    }

    mf_import_options_t opts{};
    opts.target_db_path = import_db;
    opts.schema_from_db_path = nullptr;
    opts.id_prefix = "imp-";
    opts.start_serial = 1;
    opts.decision = "imported";
    opts.reset_target_db = 1;
    opts.analyze_after_import = analyze_after_import;
    opts.fill_rule_hits = analyze_after_import ? 1 : 0;
    opts.dry_run = 0;

    int imported_count = 0;
    mf_error_t err = mf_import_header_file_with_options(
        input_file,
        &opts,
        &imported_count
    );
    if (err != MF_OK) {
        std::cerr << "mf_import_header_file_with_options failed for " << input_file
                  << ": " << mf_error_string(err) << "\n";
        return 10;
    }

    if (imported_count != 2) {
        std::cerr << "unexpected imported_count=" << imported_count
                  << " for " << input_file << "\n";
        return 11;
    }

    sqlite3 *db = nullptr;
    if (sqlite3_open(import_db, &db) != SQLITE_OK) {
        std::cerr << "sqlite3_open failed for " << import_db << "\n";
        if (db) sqlite3_close(db);
        return 12;
    }

    int messages_count = 0;
    if (!query_single_int(db, "SELECT COUNT(*) FROM messages;", &messages_count)) {
        std::cerr << "failed to query messages count for " << input_file << "\n";
        sqlite3_close(db);
        return 13;
    }

    int header_entries_count = 0;
    if (!query_single_int(db, "SELECT COUNT(*) FROM header_entries;", &header_entries_count)) {
        std::cerr << "failed to query header_entries count for " << input_file << "\n";
        sqlite3_close(db);
        return 14;
    }

    if (messages_count != 2) {
        std::cerr << "unexpected messages count=" << messages_count
                  << " for " << input_file << "\n";
        sqlite3_close(db);
        return 15;
    }

    if (header_entries_count <= 0) {
        std::cerr << "unexpected header_entries count=" << header_entries_count
                  << " for " << input_file << "\n";
        sqlite3_close(db);
        return 16;
    }

    if (analyze_after_import) {
        std::vector<MessageRow> rows;
        if (!query_message_rows(db, rows)) {
            std::cerr << "failed to query message rows for " << input_file << "\n";
            sqlite3_close(db);
            return 17;
        }

        if (rows.size() != 2) {
            std::cerr << "expected 2 message rows but got " << rows.size()
                      << " for " << input_file << "\n";
            sqlite3_close(db);
            return 18;
        }

        int rule_hits_count = 0;
        if (!query_single_int(db, "SELECT COUNT(*) FROM rule_hits;", &rule_hits_count)) {
            std::cerr << "failed to query rule_hits count for " << input_file << "\n";
            sqlite3_close(db);
            return 19;
        }

        int score_hits_count = 0;
        if (!query_single_int(db, "SELECT COUNT(*) FROM rule_hits WHERE phase='score';", &score_hits_count)) {
            std::cerr << "failed to query score rule_hits count for " << input_file << "\n";
            sqlite3_close(db);
            return 20;
        }

        int allow_hits_count = 0;
        if (!query_single_int(db, "SELECT COUNT(*) FROM rule_hits WHERE phase='allow';", &allow_hits_count)) {
            std::cerr << "failed to query allow rule_hits count for " << input_file << "\n";
            sqlite3_close(db);
            return 21;
        }

        int deny_hits_count = 0;
        if (!query_single_int(db, "SELECT COUNT(*) FROM rule_hits WHERE phase='deny';", &deny_hits_count)) {
            std::cerr << "failed to query deny rule_hits count for " << input_file << "\n";
            sqlite3_close(db);
            return 22;
        }

        const bool is_score_rc = (std::string(rc_path) == "tests/data/test-score.rc");
        const bool is_allowdeny_rc = (std::string(rc_path) == "tests/data/test-allow-deny.rc");

        if (is_score_rc) {
            if (rows[0].decision != "pass" || rows[1].decision != "pass") {
                std::cerr << "expected pass/pass for score scenario but got '"
                          << rows[0].decision << "'/ '" << rows[1].decision
                          << "' for " << input_file << "\n";
                sqlite3_close(db);
                return 30;
            }

            if (rows[0].final_score != 50 || rows[1].final_score != 50) {
                std::cerr << "expected final_score=50/50 for score scenario but got "
                          << rows[0].final_score << "/" << rows[1].final_score
                          << " for " << input_file << "\n";
                sqlite3_close(db);
                return 31;
            }

            if (score_hits_count < 6) {
                std::cerr << "expected at least 6 score rule_hits but got "
                          << score_hits_count << " for " << input_file << "\n";
                sqlite3_close(db);
                return 32;
            }

            if (allow_hits_count != 0 || deny_hits_count != 0) {
                std::cerr << "expected allow_hits=0 and deny_hits=0 for score scenario but got "
                          << allow_hits_count << "/" << deny_hits_count
                          << " for " << input_file << "\n";
                sqlite3_close(db);
                return 33;
            }
        }

        if (is_allowdeny_rc) {
            if (rows[0].decision != "allow" || rows[1].decision != "deny") {
                std::cerr << "expected allow/deny for allow-deny scenario but got '"
                          << rows[0].decision << "'/ '" << rows[1].decision
                          << "' for " << input_file << "\n";
                sqlite3_close(db);
                return 34;
            }

            if (rows[0].final_score != 0 || rows[1].final_score != 0) {
                std::cerr << "expected final_score=0/0 for allow-deny scenario but got "
                          << rows[0].final_score << "/" << rows[1].final_score
                          << " for " << input_file << "\n";
                sqlite3_close(db);
                return 35;
            }

            if (allow_hits_count < 1 || deny_hits_count < 1) {
                std::cerr << "expected at least 1 allow and 1 deny rule_hit but got "
                          << allow_hits_count << "/" << deny_hits_count
                          << " for " << input_file << "\n";
                sqlite3_close(db);
                return 36;
            }
        }

        std::cout << "ANALYZE scenario=" << scenario_name
                  << " file=" << input_file
                  << " msg1=" << rows[0].msg_log_id << ":" << rows[0].decision << ":" << rows[0].final_score
                  << " msg2=" << rows[1].msg_log_id << ":" << rows[1].decision << ":" << rows[1].final_score
                  << " rule_hits=" << rule_hits_count
                  << " score_hits=" << score_hits_count
                  << " allow_hits=" << allow_hits_count
                  << " deny_hits=" << deny_hits_count
                  << "\n";
    }

    sqlite3_close(db);

    std::cout << "OK scenario=" << scenario_name
              << " file=" << input_file
              << " imported_count=" << imported_count
              << " messages=" << messages_count
              << " header_entries=" << header_entries_count
              << " analyze=" << analyze_after_import
              << "\n";

    return 0;
}

// Hier neu 20.04.2026

static int run_open_db_check(
    const char *db_path,
    int expected_messages,
    int min_header_entries,
    int min_rule_hits
) {
    mf_error_t err = mf_open_existing_db(db_path, 1);
    if (err != MF_OK) {
        std::cerr << "mf_open_existing_db failed for " << db_path
                  << ": " << mf_error_string(err) << "\n";
        return 40;
    }

    int messages = 0;
    int header_entries = 0;
    int rule_hits = 0;

    err = mf_get_db_counts(&messages, &header_entries, &rule_hits);
    if (err != MF_OK) {
        std::cerr << "mf_get_db_counts failed for " << db_path
                  << ": " << mf_error_string(err) << "\n";
        mf_close_existing_db();
        return 41;
    }

    if (messages != expected_messages ||
        header_entries < min_header_entries ||
        rule_hits < min_rule_hits) {
        std::cerr << "unexpected DB counts for " << db_path
                  << ": messages=" << messages
                  << " header_entries=" << header_entries
                  << " rule_hits=" << rule_hits << "\n";
        mf_close_existing_db();
        return 42;
    }

    std::cout << "OPENDB file=" << db_path
              << " messages=" << messages
              << " header_entries=" << header_entries
              << " rule_hits=" << rule_hits
              << "\n";

    mf_close_existing_db();
    return 0;
}

// Helper Funktion Reader-Check 

static int run_message_reader_check(
    const char *db_path,
    int expected_count,
    const char *expected_msg1_id,
    const char *expected_msg1_decision,
    int expected_msg1_score,
    const char *expected_msg2_id,
    const char *expected_msg2_decision,
    int expected_msg2_score
) {
    mf_error_t err = mf_open_existing_db(db_path, 1);
    if (err != MF_OK) {
        std::cerr << "mf_open_existing_db failed for " << db_path
                  << ": " << mf_error_string(err) << "\n";
        return 50;
    }

    int count = 0;
    err = mf_get_message_count(&count);
    if (err != MF_OK) {
        std::cerr << "mf_get_message_count failed for " << db_path
                  << ": " << mf_error_string(err) << "\n";
        mf_close_existing_db();
        return 51;
    }

    if (count != expected_count) {
        std::cerr << "unexpected message count for " << db_path
                  << ": " << count << "\n";
        mf_close_existing_db();
        return 52;
    }

    mf_message_summary_t msg1{};
    err = mf_get_message_summary_at(0, &msg1);
    if (err != MF_OK) {
        std::cerr << "mf_get_message_summary_at(0) failed for " << db_path
                  << ": " << mf_error_string(err) << "\n";
        mf_close_existing_db();
        return 53;
    }

    mf_message_summary_t msg2{};
    err = mf_get_message_summary_at(1, &msg2);
    if (err != MF_OK) {
        std::cerr << "mf_get_message_summary_at(1) failed for " << db_path
                  << ": " << mf_error_string(err) << "\n";
        mf_close_existing_db();
        return 54;
    }

    if (std::string(msg1.msg_log_id) != expected_msg1_id ||
        std::string(msg1.decision) != expected_msg1_decision ||
        msg1.final_score != expected_msg1_score) {
        std::cerr << "unexpected first message summary for " << db_path
                  << ": " << msg1.msg_log_id << "/" << msg1.decision
                  << "/" << msg1.final_score << "\n";
        mf_close_existing_db();
        return 55;
    }

    if (std::string(msg2.msg_log_id) != expected_msg2_id ||
        std::string(msg2.decision) != expected_msg2_decision ||
        msg2.final_score != expected_msg2_score) {
        std::cerr << "unexpected second message summary for " << db_path
                  << ": " << msg2.msg_log_id << "/" << msg2.decision
                  << "/" << msg2.final_score << "\n";
        mf_close_existing_db();
        return 56;
    }

    std::cout << "READER file=" << db_path
              << " count=" << count
              << " msg1=" << msg1.msg_log_id << ":" << msg1.decision << ":" << msg1.final_score
              << " msg2=" << msg2.msg_log_id << ":" << msg2.decision << ":" << msg2.final_score
              << "\n";

    mf_close_existing_db();
    return 0;
}

static int run_header_reader_check(
    const char *db_path,
    const char *msg_log_id,
    int expected_count,
    const char *expected_tag0,
    const char *expected_tag1
) {
    mf_error_t err = mf_open_existing_db(db_path, 1);
    if (err != MF_OK) {
        std::cerr << "mf_open_existing_db failed for " << db_path
                  << ": " << mf_error_string(err) << "\n";
        return 60;
    }

    int count = 0;
    err = mf_get_header_count_for_message(msg_log_id, &count);
    if (err != MF_OK) {
        std::cerr << "mf_get_header_count_for_message failed for " << db_path
                  << " msg_log_id=" << msg_log_id
                  << ": " << mf_error_string(err) << "\n";
        mf_close_existing_db();
        return 61;
    }

    if (count != expected_count) {
        std::cerr << "unexpected header count for " << db_path
                  << " msg_log_id=" << msg_log_id
                  << ": " << count << "\n";
        mf_close_existing_db();
        return 62;
    }

    mf_header_entry_t hdr0{};
    err = mf_get_header_at(msg_log_id, 0, &hdr0);
    if (err != MF_OK) {
        std::cerr << "mf_get_header_at(0) failed for " << db_path
                  << " msg_log_id=" << msg_log_id
                  << ": " << mf_error_string(err) << "\n";
        mf_close_existing_db();
        return 63;
    }

    mf_header_entry_t hdr1{};
    err = mf_get_header_at(msg_log_id, 1, &hdr1);
    if (err != MF_OK) {
        std::cerr << "mf_get_header_at(1) failed for " << db_path
                  << " msg_log_id=" << msg_log_id
                  << ": " << mf_error_string(err) << "\n";
        mf_close_existing_db();
        return 64;
    }

    if (std::string(hdr0.msg_log_id) != msg_log_id ||
        hdr0.ordinal != 1 ||
        std::string(hdr0.tag) != expected_tag0) {
        std::cerr << "unexpected first header for " << db_path
                  << " msg_log_id=" << msg_log_id
                  << ": " << hdr0.msg_log_id << "/" << hdr0.ordinal
                  << "/" << hdr0.tag << "\n";
        mf_close_existing_db();
        return 65;
    }

    if (std::string(hdr1.msg_log_id) != msg_log_id ||
        hdr1.ordinal != 2 ||
        std::string(hdr1.tag) != expected_tag1) {
        std::cerr << "unexpected second header for " << db_path
                  << " msg_log_id=" << msg_log_id
                  << ": " << hdr1.msg_log_id << "/" << hdr1.ordinal
                  << "/" << hdr1.tag << "\n";
        mf_close_existing_db();
        return 66;
    }

    std::cout << "HEADERREADER file=" << db_path
              << " msg=" << msg_log_id
              << " count=" << count
              << " hdr0=" << hdr0.ordinal << ":" << hdr0.tag
              << " hdr1=" << hdr1.ordinal << ":" << hdr1.tag
              << "\n";

    mf_close_existing_db();
    return 0;
}

// Ende Reader-Check

int main() {
    int rc = run_import_check(
        "score-lf",
        "tests/data/sample-mailheader.log",
        "build/test-import-score-lf.sqlite3",
        1,
        "tests/data/test-score.rc"
    );
    if (rc != 0) {
        mf_shutdown();
        return rc;
    }

    rc = run_import_check(
        "allowdeny-lf",
        "tests/data/sample-mailheader.log",
        "build/test-import-allowdeny-lf.sqlite3",
        1,
        "tests/data/test-allow-deny.rc"
    );
    if (rc != 0) {
        mf_shutdown();
        return rc;
    }

    rc = run_import_check(
        "score-crlf",
        "build/sample-mailheader-crlf.log",
        "build/test-import-score-crlf.sqlite3",
        1,
        "tests/data/test-score.rc"
    );
    if (rc != 0) {
        mf_shutdown();
        return rc;
    }

// Neu
    rc = run_open_db_check(
        "build/test-import-score-lf.sqlite3",
        2,
        1,
        1
    );
    if (rc != 0) {
        return rc;
    }

    rc = run_open_db_check(
        "build/test-import-allowdeny-lf.sqlite3",
        2,
        1,
        1
    );
    if (rc != 0) {
        return rc;
    }

    rc = run_open_db_check(
        "build/test-import-score-crlf.sqlite3",
        2,
        1,
        1
    );
    if (rc != 0) {
        return rc;
    }

    rc = run_message_reader_check(
        "build/test-import-score-lf.sqlite3",
        2,
        "imp-1", "pass", 50,
        "imp-2", "pass", 50
    );
    if (rc != 0) {
        return rc;
    }

    rc = run_message_reader_check(
        "build/test-import-allowdeny-lf.sqlite3",
        2,
        "imp-1", "allow", 0,
        "imp-2", "deny", 0
    );
    if (rc != 0) {
        return rc;
    }

    rc = run_message_reader_check(
        "build/test-import-score-crlf.sqlite3",
        2,
        "imp-1", "pass", 50,
        "imp-2", "pass", 50
    );
    if (rc != 0) {
        return rc;
    }

    rc = run_header_reader_check(
        "build/test-import-score-lf.sqlite3",
        "imp-1",
        8,
        "Return-path",
        "Envelope-To"
    );
    if (rc != 0) {
        return rc;
    }

    rc = run_header_reader_check(
        "build/test-import-allowdeny-lf.sqlite3",
        "imp-1",
        8,
        "Return-path",
        "Envelope-To"
    );
    if (rc != 0) {
        return rc;
    }

    rc = run_header_reader_check(
        "build/test-import-score-crlf.sqlite3",
        "imp-1",
        8,
        "Return-path",
        "Envelope-To"
    );
    if (rc != 0) {
        return rc;
    }

    mf_shutdown();
    return 0;
}
