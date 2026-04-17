#include "mf_core.h"
#include "mf_types.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>

int main() {
    mf_config_t cfg{};
    cfg.db_path = "build/test-runtime.sqlite3";
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
    opts.target_db_path = "build/test-import.sqlite3";
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

    mf_shutdown();
    std::cout << "OK imported_count=" << imported_count << "\n";
    return 0;
}
