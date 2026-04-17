#include "mf_core.h"
#include "mf_types.h"

#include <sqlite3.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

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

static int run_import_check(const char *input_file, const char *import_db, int analyze_after_import) {
    std::remove(import_db);

    mf_import_options_t opts{};
    opts.target_db_path = import_db;
    opts.schema_from_db_path = nullptr;
    opts.id_prefix = "imp-";
    opts.start_serial = 1;
    opts.decision = "imported";
    opts.reset_target_db = 1;
    opts.analyze_after_import = analyze_after_import;
    opts.fill_rule_hits = 0;
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
        std::string decision;
        if (!query_single_text(
                db,
                "SELECT decision FROM messages ORDER BY msg_log_id LIMIT 1;",
                decision
            )) {
            std::cerr << "failed to query decision for " << input_file << "\n";
            sqlite3_close(db);
            return 17;
        }

        if (decision.empty()) {
            std::cerr << "empty decision for " << input_file << "\n";
            sqlite3_close(db);
            return 18;
        }

        if (decision != "pass" &&
            decision != "deny" &&
            decision != "score-deny" &&
            decision != "deny-maxlength" &&
            decision != "allow" &&
            decision != "duplicate" &&
            decision != "deny-after-allow") {
            std::cerr << "unexpected decision='" << decision
                      << "' for " << input_file << "\n";
            sqlite3_close(db);
            return 19;
        }

        int final_score = 0;
        if (!query_single_int(
                db,
                "SELECT final_score FROM messages ORDER BY msg_log_id LIMIT 1;",
                &final_score
            )) {
            std::cerr << "failed to query final_score for " << input_file << "\n";
            sqlite3_close(db);
            return 20;
        }
// new
        int rule_hits_count = 0;
        if (!query_single_int(
                db,
                "SELECT COUNT(*) FROM rule_hits;",
                &rule_hits_count
            )) {
            std::cerr << "failed to query rule_hits count for " << input_file << "\n";
            sqlite3_close(db);
            return 21;
        }

        if (rule_hits_count <= 0) {
            std::cerr << "expected rule_hits > 0 but got " << rule_hits_count
                      << " for " << input_file << "\n";
            sqlite3_close(db);
            return 22;
        }

        std::cout << "ANALYZE file=" << input_file
                  << " decision=" << decision
                  << " final_score=" << final_score
                  << " rule_hits=" << rule_hits_count
                  << "\n";
// 
        std::cout << "ANALYZE file=" << input_file
                  << " decision=" << decision
                  << " final_score=" << final_score
                  << "\n";

    } // Ende if analyze

    sqlite3_close(db);

    std::cout << "OK file=" << input_file
              << " imported_count=" << imported_count
              << " messages=" << messages_count
              << " header_entries=" << header_entries_count
              << " analyze=" << analyze_after_import
              << "\n";

    return 0;
}

int main() {
    const char *runtime_db = "build/test-runtime.sqlite3";

    std::remove(runtime_db);

    mf_config_t cfg{};
    cfg.db_path = runtime_db;
    cfg.rc_path = "tests/data/test-analysis.rc";
    cfg.policy_dir = ".";
    cfg.enable_sqlite_logging = 1;
    cfg.enable_rule_hits = 0;
    cfg.enable_explanations = 0;
    cfg.create_additive_tables = 0;
    cfg.app_id = "smoke-test";
    cfg.device_id = "ci";

    mf_error_t err = mf_init(&cfg);
    if (err != MF_OK) {
        std::cerr << "mf_init failed: " << mf_error_string(err) << "\n";
        return 1;
    }

    int rc = run_import_check(
        "tests/data/sample-mailheader.log",
        "build/test-import-lf.sqlite3",1
    );
    if (rc != 0) {
        mf_shutdown();
        return rc;
    }

    rc = run_import_check(
        "build/sample-mailheader-crlf.log",
        "build/test-import-crlf.sqlite3",0
    );
    if (rc != 0) {
        mf_shutdown();
        return rc;
    }

    mf_shutdown();
    return 0;
}
