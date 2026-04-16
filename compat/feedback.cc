#include "feedback.h"
#include "mf_log.h"

void mf_feedback_info(const char *msg) {
    mf_log_info(msg ? msg : "");
}

void mf_feedback_error(const char *msg) {
    mf_log_error(msg ? msg : "");
}
