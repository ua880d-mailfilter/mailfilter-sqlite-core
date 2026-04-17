#include "mf_core.h"
#include "mf_db_open.h"
#include "mf_seq_header_import.h"
#include "mf_header_db_import.h"
#include "preferences.hh"
#include "header/header.hh"
#include "mf_header_parse_utils.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <utility>

std::string mf_build_result_json(const char *decision, int final_score, const char *msg_log_id, const char *message_id);

namespace {
    static bool g_initialized = false;
    static mf_config_t g_cfg{};

    static mf_error_t mf_prepare_analysis_preferences(const mf_config_t *cfg);

    static char *mf_strdup_safe(const char *s) {
        if (!s) return nullptr;
        const size_t n = std::strlen(s) + 1;
        char *p = static_cast<char*>(std::malloc(n));
        if (!p) return nullptr;
        std::memcpy(p, s, n);
        return p;
    }

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

        mf_result_t result{};
        err = mf_analyze_header_text(raw_headers, &result);
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
        if (!cfg) {
            return MF_ERR_INVALID_ARG;
        }

        if (!cfg->rc_path || !*cfg->rc_path) {
            return MF_ERR_RC_LOAD;
        }

        Preferences &prefs = Preferences::Instance();
        prefs.init();
        prefs.set_rc_file(cfg->rc_path);

        if (!prefs.open(cfg->rc_path)) {
            return MF_ERR_RC_LOAD;
        }

        if (!prefs.load()) {
            return MF_ERR_RC_LOAD;
        }

        return MF_OK;
    }

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

}

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

    /* Phase 1:
       Header wurde jetzt real aufgebaut.
       Die eigentliche Weeder-/Score-Logik folgt im nächsten Schritt. */
    out_result->decision = mf_strdup_safe("pass");
    out_result->final_score = 0;

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

void mf_shutdown(void) {
    if (!g_initialized) return;
    mf_db_close_existing();
    std::memset(&g_cfg, 0, sizeof(g_cfg));
    g_initialized = false;
}
