#include <string>

std::string mf_build_result_json(const char *decision, int final_score, const char *msg_log_id, const char *message_id) {
    std::string json = "{\"decision\":\"";
    json += decision ? decision : "";
    json += "\",\"final_score\":" + std::to_string(final_score);
    json += ",\"msg_log_id\":\"";
    json += msg_log_id ? msg_log_id : "";
    json += "\",\"message_id\":\"";
    json += message_id ? message_id : "";
    json += "\"}";
    return json;
}
