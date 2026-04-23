#include "mf_core.h"
#include "mf_types.h"
#include <sqlite3.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>


static int run_existing_db_counts_check(const char *db_path)
{
    mf_error_t err = mf_open_existing_db(db_path, 1);
    if (err != MF_OK) {
        std::cerr << "mf_open_existing_db failed for " << db_path
                  << ": " << mf_error_string(err) << "\\n";
        return 10;
    }

    int messages = 0;
    int header_entries = 0;
    int rule_hits = 0;

    err = mf_get_db_counts(&messages, &header_entries, &rule_hits);
    if (err != MF_OK) {
        std::cerr << "mf_get_db_counts failed for " << db_path
                  << ": " << mf_error_string(err) << "\\n";
        mf_close_existing_db();
        return 11;
    }

    if (messages != 12 || header_entries != 127 || rule_hits != 0) {
        std::cerr << "unexpected counts for " << db_path
                  << ": " << messages << "/" << header_entries << "/" << rule_hits << "\\n";
        mf_close_existing_db();
        return 12;
    }

    std::cout << "EXISTINGDBCOUNTS file=" << db_path
              << " messages=" << messages
              << " header_entries=" << header_entries
              << " rule_hits=" << rule_hits
              << "\\n";

    mf_close_existing_db();
    return 0;
}

static int run_existing_db_message_check(
    const char *db_path,
    const char *msg_log_id,
    const char *expected_decision,
    const char *expected_subject_contains,
    const char *expected_from_contains,
    const char *expected_to
)
{
    mf_error_t err = mf_open_existing_db(db_path, 1);
    if (err != MF_OK) {
        std::cerr << "mf_open_existing_db failed for " << db_path
                  << ": " << mf_error_string(err) << "\\n";
        return 20;
    }

    mf_message_summary_t msg{};
    err = mf_get_message_summary_by_id(msg_log_id, &msg);
    if (err != MF_OK) {
        std::cerr << "mf_get_message_summary_by_id failed for " << db_path
                  << " msg_log_id=" << msg_log_id
                  << ": " << mf_error_string(err) << "\\n";
        mf_close_existing_db();
        return 21;
    }

    if (std::string(msg.msg_log_id) != msg_log_id ||
        std::string(msg.decision) != expected_decision ||
        std::string(msg.subject).find(expected_subject_contains) == std::string::npos ||
        std::string(msg.from_addr).find(expected_from_contains) == std::string::npos ||
        std::string(msg.to_addr) != expected_to) {
        std::cerr << "unexpected message for " << db_path
                  << " msg_log_id=" << msg_log_id
                  << ": " << msg.msg_log_id << "/" << msg.decision
                  << "/" << msg.subject << "/" << msg.from_addr
                  << "/" << msg.to_addr << "\\n";
        mf_close_existing_db();
        return 22;
    }

    std::cout << "EXISTINGDBMESSAGE file=" << db_path
              << " msg=" << msg.msg_log_id
              << ":" << msg.decision
              << ":" << msg.subject
              << "\\n";

    mf_close_existing_db();
    return 0;
}

static int run_existing_db_header_check(
    const char *db_path,
    const char *msg_log_id,
    int expected_header_count
)
{
    mf_error_t err = mf_open_existing_db(db_path, 1);
    if (err != MF_OK) {
        std::cerr << "mf_open_existing_db failed for " << db_path
                  << ": " << mf_error_string(err) << "\\n";
        return 30;
    }

    int count = 0;
    err = mf_get_header_count_for_message(msg_log_id, &count);
    if (err != MF_OK) {
        std::cerr << "mf_get_header_count_for_message failed for " << db_path
                  << " msg_log_id=" << msg_log_id
                  << ": " << mf_error_string(err) << "\\n";
        mf_close_existing_db();
        return 31;
    }

    if (count != expected_header_count) {
        std::cerr << "unexpected header count for " << db_path
                  << " msg_log_id=" << msg_log_id
                  << ": " << count << "\\n";
        mf_close_existing_db();
        return 32;
    }

    mf_header_entry_t hdr0{};
    err = mf_get_header_at(msg_log_id, 0, &hdr0);
    if (err != MF_OK) {
        std::cerr << "mf_get_header_at(0) failed for " << db_path
                  << " msg_log_id=" << msg_log_id
                  << ": " << mf_error_string(err) << "\\n";
        mf_close_existing_db();
        return 33;
    }

    if (std::string(hdr0.msg_log_id) != msg_log_id || hdr0.ordinal != 1) {
        std::cerr << "unexpected first header for " << db_path
                  << " msg_log_id=" << msg_log_id
                  << ": " << hdr0.msg_log_id << "/" << hdr0.ordinal
                  << "/" << hdr0.tag << "/" << hdr0.body << "\\n";
        mf_close_existing_db();
        return 34;
    }

    std::cout << "EXISTINGDBHEADER file=" << db_path
              << " msg=" << msg_log_id
              << " count=" << count
              << " first_header=" << hdr0.tag
              << "\\n";

    mf_close_existing_db();
    return 0;
}

int main()
{
    mf_config_t cfg{};
    cfg.app_name = "existing-db-smoke-test";

    mf_error_t err = mf_init(&cfg);
    if (err != MF_OK) {
        std::cerr << "mf_init failed: " << mf_error_string(err) << "\\n";
        return 1;
    }

    int rc = run_existing_db_counts_check("tests/data/mailheader-test.sqlite3");
    if (rc != 0) {
        return rc;
    }

    rc = run_existing_db_message_check(
        "tests/data/mailheader-test.sqlite3",
        "tstham-1",
        "pass",
        "Rechnung",
        "Pollin",
        "test@example.net"
    );
    if (rc != 0) {
        return rc;
    }

    rc = run_existing_db_message_check(
        "tests/data/mailheader-test.sqlite3",
        "tstspam-1",
        "deny",
        "Sale",
        "Tipsy Deals",
        "test@example.net"
    );
    if (rc != 0) {
        return rc;
    }

    rc = run_existing_db_header_check(
        "tests/data/mailheader-test.sqlite3",
        "tstham-1",
        10
    );
    if (rc != 0) {
        return rc;
    }

    rc = run_existing_db_header_check(
        "tests/data/mailheader-test.sqlite3",
        "tstspam-1",
        12
    );
    if (rc != 0) {
        return rc;
    }

    mf_shutdown();
    return 0;
}

