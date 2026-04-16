#ifndef MF_ERROR_H
#define MF_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum mf_error_t {
    MF_OK = 0,

    MF_ERR_INVALID_ARG = 1,
    MF_ERR_INIT = 2,
    MF_ERR_ALREADY_INITIALIZED = 3,
    MF_ERR_NOT_INITIALIZED = 4,

    MF_ERR_DB_OPEN = 10,
    MF_ERR_DB_SCHEMA = 11,
    MF_ERR_DB_WRITE = 12,
    MF_ERR_DB_READONLY = 13,

    MF_ERR_RC_LOAD = 20,
    MF_ERR_PARSE = 21,
    MF_ERR_ANALYZE = 22,
    MF_ERR_STORE = 23,

    MF_ERR_IO = 30,
    MF_ERR_OOM = 31,
    MF_ERR_FORMAT = 32,
    MF_ERR_UNSUPPORTED = 33,

    MF_ERR_INTERNAL = 255
} mf_error_t;

const char *mf_error_string(mf_error_t err);

#ifdef __cplusplus
}
#endif

#endif
