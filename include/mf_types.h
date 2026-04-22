#ifndef MF_TYPES_H
#define MF_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mf_config_t {
    const char *db_path;
    const char *rc_path;
    const char *policy_dir;

    int enable_sqlite_logging;
    int enable_rule_hits;
    int enable_explanations;
    int create_additive_tables;

    const char *app_id;
    const char *device_id;
} mf_config_t;

typedef struct mf_result_t {
    int final_score;
    char *decision;
    char *msg_log_id;
    char *message_id;
    char *result_json;
} mf_result_t;

typedef struct mf_version_t {
    int major;
    int minor;
    int patch;
} mf_version_t;

typedef struct mf_import_stats_t {
    int total_blocks_seen;
    int total_blocks_parsed;
    int total_blocks_analyzed;
    int total_blocks_failed;
} mf_import_stats_t;

typedef struct mf_import_options_t {
    /* Ziel-DB, in die importiert wird */
    const char *target_db_path;

    /* Optional: Schema aus vorhandener DB übernehmen */
    const char *schema_from_db_path;

    /* Optional: Prefix für erzeugte msg_log_id, z. B. "imp-" */
    const char *id_prefix;

    /* Optionaler Startwert für Laufnummern */
    int start_serial;

    /* Optionaler Decision-Text für importierte Datensätze */
    const char *decision;

    /* 1 = Ziel-DB vor dem Import zurücksetzen */
    int reset_target_db;

    /* 1 = Import direkt analysieren */
    int analyze_after_import;

    /* 1 = rule_hits beim Import füllen, falls Analyse aktiv */
    int fill_rule_hits;

    /* 1 = nur parsen/validieren, nichts schreiben */
    int dry_run;
} mf_import_options_t;

typedef struct mf_message_summary_t {
    char msg_log_id[64];
    char decision[32];
    int final_score;
    char subject[512];
    char date_hdr[128];
    char created_at[32];
    char from_addr[256];
    char to_addr[256];
} mf_message_summary_t;

typedef struct mf_header_entry_t {
    char msg_log_id[64];
    int ordinal;
    char tag[128];
    char body[4096];
} mf_header_entry_t;

typedef struct mf_rule_hit_t {
    char msg_log_id[64];
    char phase[32];
    char expression[512];
    int is_negative;
    int matched;
    char header_tag[128];
    char header_body[4096];
    int normalized_subject;
    int score_delta;
} mf_rule_hit_t;

typedef struct mf_rule_hit_agg_t {
    char expression[512];
    int hit_count;
} mf_rule_hit_agg_t;


#ifdef __cplusplus
}
#endif

#endif
