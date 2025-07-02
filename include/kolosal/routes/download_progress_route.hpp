#ifndef KOLOSAL_DOWNLOAD_PROGRESS_ROUTE_HPP
#define KOLOSAL_DOWNLOAD_PROGRESS_ROUTE_HPP

#include "route_interface.hpp"
#include <string>

namespace kolosal {

    class DownloadProgressRoute : public IRoute {
    public:
        bool match(const std::string& method, const std::string& path) override;
        void handle(SocketType sock, const std::string& body) override;
        
    private:
        // Store the matched path to extract model ID later
        mutable std::string matched_path_;
        
        // Extract model ID from path like /download-progress/{model_id}
        std::string extractModelId(const std::string& path);
    };

} // namespace kolosal

#endif // KOLOSAL_DOWNLOAD_PROGRESS_ROUTE_HPP
