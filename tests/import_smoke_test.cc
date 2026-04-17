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

int main() {
    const char *runtime_db = "build/test-runtime.sqlite3";
    const char *import_db  = "build/test-import.sqlite3";

    std::remove(runtime_db);
    std::remove(import_db);

    mf_config_t cfg{};
    cfg.db_path = runtime_db;
    cfg.rc_path = "tests/data/dummy.rc";
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

    mf_import_options_t opts{};
    opts.target_db_path = import_db;
    opts.schema_from_db_path = nullptr;
    opts.id_prefix = "imp-";
    opts.start_serial = 1;
    opts.decision = "imported";
    opts.reset_target_db = 1;
    opts.analyze_after_import = 0;
    opts.fill_rule_hits = 0;
    opts.dry_run = 0;

    int imported_count = 0;
    err = mf_import_header_file_with_options(
        "tests/data/sample-mailheader.log",
        &opts,
        &imported_count
    );
    if (err != MF_OK) {
        std::cerr << "mf_import_header_file_with_options failed: "
                  << mf_error_string(err) << "\n";
        mf_shutdown();
        return 2;
    }

    if (imported_count != 2) {
        std::cerr << "unexpected imported_count=" << imported_count << "\n";
        mf_shutdown();
        return 3;
    }

    sqlite3 *db = nullptr;
    if (sqlite3_open(import_db, &db) != SQLITE_OK) {
        std::cerr << "sqlite3_open failed for import db\n";
        if (db) sqlite3_close(db);
        mf_shutdown();
        return 4;
    }

    int messages_count = 0;
    if (!query_single_int(db, "SELECT COUNT(*) FROM MESSAGES;", &messages_count)) {
        std::cerr << "failed to query MESSAGES count\n";
        sqlite3_close(db);
        mf_shutdown();
        return 5;
    }

    int header_entries_count = 0;
    if (!query_single_int(db, "SELECT COUNT(*) FROM HEADER_ENTRIES;", &header_entries_count)) {
        std::cerr << "failed to query HEADER_ENTRIES count\n";
        sqlite3_close(db);
        mf_shutdown();
        return 6;
    }

    if (messages_count != 2) {
        std::cerr << "unexpected MESSAGES count=" << messages_count << "\n";
        sqlite3_close(db);
        mf_shutdown();
        return 7;
    }

    if (header_entries_count <= 0) {
        std::cerr << "unexpected HEADER_ENTRIES count=" << header_entries_count << "\n";
        sqlite3_close(db);
        mf_shutdown();
        return 8;
    }

    sqlite3_close(db);
    mf_shutdown();

    std::cout << "OK imported_count=" << imported_count
              << " messages=" << messages_count
              << " header_entries=" << header_entries_count
              << "\n";
    return 0;
}
