#include "kolosal/utils.hpp"

namespace ResponseContext {
    // Thread-local storage for global headers (e.g., from auth middleware)
    thread_local std::map<std::string, std::string> globalHeaders;
    
    void setGlobalHeaders(const std::map<std::string, std::string>& headers) {
        globalHeaders = headers;
    }
    
    const std::map<std::string, std::string>& getGlobalHeaders() {
        return globalHeaders;
    }
    
    void clearGlobalHeaders() {
        globalHeaders.clear();
    }
}
