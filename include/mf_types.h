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

#ifdef __cplusplus
}
#endif

#endif
