#include "mf_core.h"
#include "mf_db_open.h"
#include "mf_seq_header_import.h"
#include "mf_header_db_import.h"
#include "preferences.hh"
#include "header/header.hh"
#include "mf_header_parse_utils.h"
#include "weeder.hh"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <utility>

std::string mf_build_result_json(const char *decision, int final_score, const char *msg_log_id, const char *message_id);

namespace {
    static bool g_initialized = false;
    static mf_config_t g_cfg{};
    static std::string g_loaded_analysis_rc_path;
    static bool g_analysis_prefs_loaded = false;

    static mf_error_t mf_prepare_analysis_preferences(const mf_config_t *cfg);

    static char *mf_strdup_safe(const char *s) {
        if (!s) return nullptr;
        const size_t n = std::strlen(s) + 1;
        char *p = static_cast<char*>(std::malloc(n));
        if (!p) return nullptr;
        std::memcpy(p, s, n);
        return p;
    }

    static mf_error_t mf_build_header_from_text(
        const char *raw_headers,
        Header **out_header
    );

    static mf_error_t mf_analyze_header_object(
        Header *hdr,
        Weeder &weeder,
        mf_result_t *out_result
    );

    static void mf_write_score_rule_hits(
        const mf_import_options_t *options,
        const char *msg_log_id,
        const Weeder &weeder
    );

    static void mf_write_allow_rule_hits(
        const mf_import_options_t *options,
        const char *msg_log_id,
        const Weeder &weeder
    );

    static void mf_write_deny_rule_hits(
        const mf_import_options_t *options,
        const char *msg_log_id,
        const Weeder &weeder
    );

static void mf_write_score_rule_hits(
    const mf_import_options_t *options,
    const char *msg_log_id,
    const Weeder &weeder
)
{
    if (!options || !options->target_db_path || !msg_log_id) {
        return;
    }

    const auto &hits = weeder.score_hits();
    for (const auto &hit : hits) {
        (void)mf_insert_rule_hit(
            options->target_db_path,
            msg_log_id,
            "score",
            hit.expression.c_str(),
            hit.is_negative,
            hit.matched,
            hit.header_tag.c_str(),
            hit.header_body.c_str(),
            hit.normalized_subject,
            hit.score_delta
        );
    }
}


static void mf_write_allow_rule_hits(
    const mf_import_options_t *options,
    const char *msg_log_id,
    const Weeder &weeder
)
{
    if (!options || !options->target_db_path || !msg_log_id) {
        return;
    }

    const auto &hits = weeder.allow_hits();
    for (const auto &hit : hits) {
        (void)mf_insert_rule_hit(
            options->target_db_path,
            msg_log_id,
            "allow",
            hit.expression.c_str(),
            hit.is_negative,
            hit.matched,
            hit.header_tag.c_str(),
            hit.header_body.c_str(),
            hit.normalized_subject,
            0
        );
    }
}

static void mf_write_deny_rule_hits(
    const mf_import_options_t *options,
    const char *msg_log_id,
    const Weeder &weeder
)
{
    if (!options || !options->target_db_path || !msg_log_id) {
        return;
    }

    const auto &hits = weeder.deny_hits();
    for (const auto &hit : hits) {
        (void)mf_insert_rule_hit(
            options->target_db_path,
            msg_log_id,
            "deny",
            hit.expression.c_str(),
            hit.is_negative,
            hit.matched,
            hit.header_tag.c_str(),
            hit.header_body.c_str(),
            hit.normalized_subject,
            0
        );
    }
}

//# Ende NEW

    static mf_error_t mf_analyze_imported_header_block(
        const char *raw_headers,
        const mf_import_options_t *options,
        int serial,
        char **out_msg_log_id
    )
    {
        if (!raw_headers || !options || !options->target_db_path) {
            return MF_ERR_INVALID_ARG;
        }

        mf_error_t err = mf_prepare_analysis_preferences(&g_cfg);
        if (err != MF_OK) {
            return err;
        }

        char *msg_log_id = nullptr;
        err = mf_import_header_text_to_db(
            raw_headers,
            options,
            serial,
            &msg_log_id
        );
        if (err != MF_OK) {
            return err;
        }

        Header *hdr = nullptr;
        err = mf_build_header_from_text(raw_headers, &hdr);
        if (err != MF_OK) {
            std::free(msg_log_id);
            return err;
        }

        mf_result_t result{};
        Weeder weeder;

        err = mf_analyze_header_object(hdr, weeder, &result);

        delete hdr;

        if (err != MF_OK) {
            std::free(msg_log_id);
            mf_free_result(&result);
            return err;
        }

        const char *decision = result.decision ? result.decision : "pass";
        const int final_score = result.final_score;

        err = mf_update_message_analysis_result(
            options->target_db_path,
            msg_log_id,
            decision,
            final_score
        );

        if (err == MF_OK && options->fill_rule_hits) {
            mf_write_score_rule_hits(options, msg_log_id, weeder);
            mf_write_allow_rule_hits(options, msg_log_id, weeder);
            mf_write_deny_rule_hits(options, msg_log_id, weeder);
        }

        if (out_msg_log_id) {
            *out_msg_log_id = msg_log_id;
        } else {
            std::free(msg_log_id);
        }

        mf_free_result(&result);
        return err;
    }

static mf_error_t mf_prepare_analysis_preferences(const mf_config_t *cfg)
{
/* Debug
    std::fprintf(stderr,
                 "DEBUG prefs: enter rc_path=%s loaded=%d cached=%s\n",
                 (cfg && cfg->rc_path) ? cfg->rc_path : "(null)",
                 g_analysis_prefs_loaded ? 1 : 0,
                 g_loaded_analysis_rc_path.empty() ? "(empty)" : g_loaded_analysis_rc_path.c_str());
*/
    if (!cfg) {
        return MF_ERR_INVALID_ARG;
    }

    if (!cfg->rc_path || !*cfg->rc_path) {
        return MF_ERR_RC_LOAD;
    }

    if (g_analysis_prefs_loaded && g_loaded_analysis_rc_path == cfg->rc_path) {
/* Debug
        std::fprintf(stderr,
                     "DEBUG prefs: cache hit rc_path=%s\n",
                     cfg->rc_path);
*/
        return MF_OK;
    }

    Preferences &prefs = Preferences::Instance();

    prefs.kill();
    prefs.set_rc_file(cfg->rc_path);

    if (!prefs.open(cfg->rc_path)) {
        return MF_ERR_RC_LOAD;
    }

    if (!prefs.load()) {
        return MF_ERR_RC_LOAD;
    }

/* Debug----------------------------------------------
    std::fprintf(stderr,
                 "DEBUG prefs: loaded rc_path=%s allow=%zu deny=%zu score=%zu\n",
                 cfg->rc_path,
                 Preferences::Instance().allow_filters()->size(),
                 Preferences::Instance().deny_filters()->size(),
                 Preferences::Instance().score_filters()->size());
*/
    g_loaded_analysis_rc_path = cfg->rc_path;
    g_analysis_prefs_loaded = true;
    return MF_OK;
}

//###

    static mf_error_t mf_build_header_from_text(
        const char *raw_headers,
        Header **out_header
    )
    {
        if (!raw_headers || !out_header) {
            return MF_ERR_INVALID_ARG;
        }

        *out_header = nullptr;

        std::vector<std::pair<std::string, std::string>> fields;
        mf_error_t err = mf_parse_headers_to_fields(raw_headers, fields);
        if (err != MF_OK) {
            return err;
        }

        Header *hdr = nullptr;
        try {
            hdr = new Header();

            for (const auto &f : fields) {
                hdr->add_entry(f.first.c_str(), f.second.c_str());
            }

            hdr->set_size(static_cast<int>(std::strlen(raw_headers)));
        } catch (const WrongMessageIDException &) {
            delete hdr;
            return MF_ERR_FORMAT;
        } catch (...) {
            delete hdr;
            return MF_ERR_INTERNAL;
        }

        *out_header = hdr;
        return MF_OK;
    }
// start
    static mf_error_t mf_analyze_header_object(
        Header *hdr,
        Weeder &weeder,
        mf_result_t *out_result
    )
    {
        if (!hdr || !out_result) {
            return MF_ERR_INVALID_ARG;
        }

        const int weed_status = weeder.is_weed(hdr);
        if (weed_status < 0) {
            return MF_ERR_INTERNAL;
        }

        out_result->final_score = weeder.final_score();
        out_result->decision = mf_strdup_safe(weeder.decision().c_str());

        if (!out_result->decision) {
            return MF_ERR_OOM;
        }

        return MF_OK;
    }

} // Ende Namespace

const char *mf_error_string(mf_error_t err) {
    switch (err) {
        case MF_OK: return "OK";
        case MF_ERR_INVALID_ARG: return "Invalid argument";
        case MF_ERR_INIT: return "Initialization failed";
        case MF_ERR_ALREADY_INITIALIZED: return "Already initialized";
        case MF_ERR_NOT_INITIALIZED: return "Not initialized";
        case MF_ERR_DB_OPEN: return "Database open failed";
        case MF_ERR_DB_SCHEMA: return "Database schema check failed";
        case MF_ERR_DB_WRITE: return "Database write failed";
        case MF_ERR_DB_READONLY: return "Database is read-only";
        case MF_ERR_RC_LOAD: return "Rule configuration load failed";
        case MF_ERR_PARSE: return "Parsing failed";
        case MF_ERR_ANALYZE: return "Analysis failed";
        case MF_ERR_STORE: return "Store failed";
        case MF_ERR_IO: return "I/O failed";
        case MF_ERR_OOM: return "Out of memory";
        case MF_ERR_FORMAT: return "Input format invalid";
        case MF_ERR_UNSUPPORTED: return "Unsupported";
        default: return "Internal error";
    }
}

mf_error_t mf_init(const mf_config_t *cfg) {
    if (!cfg || !cfg->db_path || !cfg->rc_path) {
        return MF_ERR_INVALID_ARG;
    }
    if (g_initialized) {
        return MF_ERR_ALREADY_INITIALIZED;
    }

    g_cfg = *cfg;
    g_initialized = true;
    return MF_OK;
}

int mf_is_initialized(void) {
    return g_initialized ? 1 : 0;
}

mf_error_t mf_get_version(mf_version_t *out_version) {
    if (!out_version) return MF_ERR_INVALID_ARG;
    out_version->major = 0;
    out_version->minor = 2;
    out_version->patch = 0;
    return MF_OK;
}

static mf_import_options_t mf_default_import_options() {
    mf_import_options_t opts{};
    opts.target_db_path = nullptr;
    opts.schema_from_db_path = nullptr;
    opts.id_prefix = "imp-";
    opts.start_serial = 1;
    opts.decision = "imported";
    opts.reset_target_db = 0;
    opts.analyze_after_import = 0;
    opts.fill_rule_hits = 0;
    opts.dry_run = 0;
    return opts;
}

mf_error_t mf_analyze_header_text(
    const char *header_text,
    mf_result_t *out_result
)
{
    if (!header_text || !out_result) {
        return MF_ERR_INVALID_ARG;
    }
    if (!mf_is_initialized()) {
        return MF_ERR_NOT_INITIALIZED;
    }

    std::memset(out_result, 0, sizeof(*out_result));

    mf_error_t err = mf_prepare_analysis_preferences(&g_cfg);
    if (err != MF_OK) {
        return err;
    }

    Header *hdr = nullptr;
    err = mf_build_header_from_text(header_text, &hdr);
    if (err != MF_OK) {
        return err;
    }

    Weeder weeder;
    err = mf_analyze_header_object(hdr, weeder, out_result);
    delete hdr;

    if (!out_result->decision) {
        mf_free_result(out_result);
        return MF_ERR_OOM;
    }

    return MF_OK;
}

mf_error_t mf_analyze_eml_file(const char *eml_path, mf_result_t *out_result) {
    if (!g_initialized) return MF_ERR_NOT_INITIALIZED;
    if (!eml_path || !out_result) return MF_ERR_INVALID_ARG;
    return MF_ERR_UNSUPPORTED;
}

mf_error_t mf_open_existing_db(const char *source_db_path, int open_read_only) {
    if (!mf_is_initialized()) {
        return MF_ERR_NOT_INITIALIZED;
    }
    return mf_db_open_existing(source_db_path, open_read_only);
}

mf_error_t mf_close_existing_db(void) {
    if (!mf_is_initialized()) {
        return MF_ERR_NOT_INITIALIZED;
    }
    return mf_db_close_existing();
}

mf_error_t mf_import_sequential_header_file(
    const char *input_path,
    int analyze_and_store,
    int *out_imported_count
) {
    if (!mf_is_initialized()) {
        return MF_ERR_NOT_INITIALIZED;
    }
    return mf_import_sequential_header_file_impl(
        input_path,
        analyze_and_store,
        out_imported_count
    );
}

mf_error_t mf_parse_sequential_header_block(
    const char *input_text,
    char **out_raw_headers
) {
    return mf_parse_sequential_header_block_text(input_text, out_raw_headers);
}

void mf_free_result(mf_result_t *result) {
    if (!result) return;
    std::free(result->decision);
    std::free(result->msg_log_id);
    std::free(result->message_id);
    std::free(result->result_json);
    std::memset(result, 0, sizeof(*result));
}

mf_error_t mf_clone_schema_from_db(
    const char *source_db_path,
    const char *target_db_path
) {
    if (!source_db_path || !target_db_path) {
        return MF_ERR_INVALID_ARG;
    }
    if (!mf_is_initialized()) {
        return MF_ERR_NOT_INITIALIZED;
    }
    return mf_clone_schema_from_db_impl(source_db_path, target_db_path);
}

mf_error_t mf_create_empty_db(
    const char *target_db_path
) {
    if (!target_db_path) {
        return MF_ERR_INVALID_ARG;
    }
    if (!mf_is_initialized()) {
        return MF_ERR_NOT_INITIALIZED;
    }
    return mf_create_empty_db_impl(target_db_path);
}

mf_error_t mf_import_header_file_with_options(
    const char *input_path,
    const mf_import_options_t *options,
    int *out_imported_count
) {
    if (!input_path || !options) {
        return MF_ERR_INVALID_ARG;
    }
    if (!mf_is_initialized()) {
        return MF_ERR_NOT_INITIALIZED;
    }
    if (!options->target_db_path || !*options->target_db_path) {
        return MF_ERR_INVALID_ARG;
    }
    if (out_imported_count) {
        *out_imported_count = 0;
    }

    FILE *fp = std::fopen(input_path, "rb");
    if (!fp) {
        return MF_ERR_IO;
    }

    if (options->reset_target_db && !options->dry_run) {
        std::remove(options->target_db_path);
    }

    if (!options->dry_run) {
        mf_error_t err;
        if (options->schema_from_db_path && *options->schema_from_db_path) {
            err = mf_clone_schema_from_db_impl(
                options->schema_from_db_path,
                options->target_db_path
            );
        } else {
            err = mf_create_empty_db_impl(options->target_db_path);
        }

        if (err != MF_OK) {
            std::fclose(fp);
            return err;
        }
    }

    char buffer[8192];
    std::string block;
    int imported = 0;

    auto flush_block = [&](void) -> mf_error_t {
        if (block.empty()) {
            return MF_OK;
        }

        char *raw_headers = nullptr;
        mf_error_t err = mf_parse_sequential_header_block_text(
            block.c_str(),
            &raw_headers
        );
        if (err != MF_OK) {
            return err;
        }

    if (!options->dry_run) {
        char *msg_log_id = nullptr;

        if (options->analyze_after_import) {
            err = mf_analyze_imported_header_block(
                raw_headers,
                options,
                imported + 1,
                &msg_log_id
            );
        } else {
            err = mf_import_header_text_to_db(
                raw_headers,
                options,
                imported + 1,
                &msg_log_id
            );
        }

        std::free(msg_log_id);
    }

        std::free(raw_headers);

        if (err != MF_OK) {
            return err;
        }

        ++imported;
        block.clear();
        return MF_OK;
    };

    while (std::fgets(buffer, sizeof(buffer), fp) != nullptr) {
        std::string line(buffer);

        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }

        block.append(line);
        block.push_back('\n');

        if (line == ".") {
            mf_error_t err = flush_block();
            if (err != MF_OK) {
                std::fclose(fp);
                return err;
            }
        }
    }

    if (!block.empty()) {
        mf_error_t err = flush_block();
        if (err != MF_OK) {
            std::fclose(fp);
            return err;
        }
    }

    std::fclose(fp);

    if (out_imported_count) {
        *out_imported_count = imported;
    }

    return MF_OK;
}

mf_error_t mf_import_header_text_with_options(
    const char *input_text,
    const mf_import_options_t *options,
    int *out_imported_count
) {
    if (!input_text || !options) {
        return MF_ERR_INVALID_ARG;
    }
    if (!mf_is_initialized()) {
        return MF_ERR_NOT_INITIALIZED;
    }
    if (out_imported_count) {
        *out_imported_count = 0;
    }

    char *msg_log_id = nullptr;
    mf_error_t err;

    if (options->analyze_after_import) {
        err = mf_analyze_imported_header_block(
            input_text,
            options,
            1,
            &msg_log_id
        );
    } else {
        err = mf_import_header_text_to_db(
            input_text,
            options,
            1,
            &msg_log_id
        );
    }

    std::free(msg_log_id);

    if (err == MF_OK && out_imported_count) {
        *out_imported_count = 1;
    }
    return err;
}

mf_error_t mf_validate_schema(void) {
    if (!g_initialized) return MF_ERR_NOT_INITIALIZED;
    return mf_db_validate_required_schema();
}

// Hier Open Wrapper (read count)

mf_error_t mf_get_db_counts(
    int *out_messages,
    int *out_header_entries,
    int *out_rule_hits
)
{
    return mf_db_get_counts(
        out_messages,
        out_header_entries,
        out_rule_hits
    );
}

mf_error_t mf_get_message_count(int *out_count)
{
    if (!g_initialized) {
        return MF_ERR_NOT_INITIALIZED;
    }

    return mf_db_get_message_count(out_count);
}

mf_error_t mf_get_message_summary_at(
    int index,
    mf_message_summary_t *out_summary
)
{
    if (!g_initialized) {
        return MF_ERR_NOT_INITIALIZED;
    }

    return mf_db_get_message_summary_at(index, out_summary);
}

// new 22.04.2026 ->

mf_error_t mf_get_message_summary_by_id(
    const char *msg_log_id,
    mf_message_summary_t *out_summary
)
{
    if (!g_initialized) {
        return MF_ERR_NOT_INITIALIZED;
    }

    return mf_db_get_message_summary_by_id(msg_log_id, out_summary);
}

// end new

mf_error_t mf_get_header_count_for_message(
    const char *msg_log_id,
    int *out_count
)
{
    if (!g_initialized) {
        return MF_ERR_NOT_INITIALIZED;
    }

    return mf_db_get_header_count_for_message(msg_log_id, out_count);
}

mf_error_t mf_get_header_at(
    const char *msg_log_id,
    int index,
    mf_header_entry_t *out_entry
)
{
    if (!g_initialized) {
        return MF_ERR_NOT_INITIALIZED;
    }

    return mf_db_get_header_at(msg_log_id, index, out_entry);
}

mf_error_t mf_get_rule_hit_count_for_message(
    const char *msg_log_id,
    int *out_count
)
{
    if (!g_initialized) {
        return MF_ERR_NOT_INITIALIZED;
    }

    return mf_db_get_rule_hit_count_for_message(msg_log_id, out_count);
}

mf_error_t mf_get_rule_hit_at(
    const char *msg_log_id,
    int index,
    mf_rule_hit_t *out_hit
)
{
    if (!g_initialized) {
        return MF_ERR_NOT_INITIALIZED;
    }

    return mf_db_get_rule_hit_at(msg_log_id, index, out_hit);
}

mf_error_t mf_get_rule_hit_expression_agg_count(int *out_count)
{
    if (!g_initialized) {
        return MF_ERR_NOT_INITIALIZED;
    }

    return mf_db_get_rule_hit_expression_agg_count(out_count);
}

mf_error_t mf_get_rule_hit_expression_agg_at(
    int index,
    mf_rule_hit_agg_t *out_agg
)
{
    if (!g_initialized) {
        return MF_ERR_NOT_INITIALIZED;
    }

    return mf_db_get_rule_hit_expression_agg_at(index, out_agg);
}


mf_error_t mf_get_rule_hit_expression_tag_agg_count(int *out_count)
{
    if (!g_initialized) {
        return MF_ERR_NOT_INITIALIZED;
    }

    return mf_db_get_rule_hit_expression_tag_agg_count(out_count);
}

mf_error_t mf_get_rule_hit_expression_tag_agg_at(
    int index,
    mf_rule_hit_tag_agg_t *out_agg
)
{
    if (!g_initialized) {
        return MF_ERR_NOT_INITIALIZED;
    }

    return mf_db_get_rule_hit_expression_tag_agg_at(index, out_agg);
}

// Ende Open Wrapper

void mf_shutdown(void) {
/* Debug
    std::fprintf(stderr,
                 "DEBUG shutdown: initialized=%d loaded=%d cached=%s\n",
                 g_initialized ? 1 : 0,
                 g_analysis_prefs_loaded ? 1 : 0,
                 g_loaded_analysis_rc_path.empty() ? "(empty)" : g_loaded_analysis_rc_path.c_str());
*/
    if (!g_initialized) return;
    mf_db_close_existing();
    Preferences::Instance().kill();
    g_loaded_analysis_rc_path.clear();
    g_analysis_prefs_loaded = false;
// Debug    
//std::fprintf(stderr, "DEBUG shutdown: cache cleared\n");
// Ende Debug
    std::memset(&g_cfg, 0, sizeof(g_cfg));
    g_initialized = false;
}
