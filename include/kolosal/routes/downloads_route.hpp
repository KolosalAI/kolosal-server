#ifndef KOLOSAL_DOWNLOADS_ROUTE_HPP
#define KOLOSAL_DOWNLOADS_ROUTE_HPP

#include "route_interface.hpp"
#include <string>

namespace kolosal {

    class DownloadsRoute : public IRoute {
    public:
        bool match(const std::string& method, const std::string& path) override;
        void handle(SocketType sock, const std::string& body) override;
        
    private:
        // Store the matched path to extract model ID later
        mutable std::string matched_path_;
        
        // Extract model ID from path like /downloads/{model_id}
        std::string extractModelId(const std::string& path);
        
        // Handle single download progress request (GET)
        void handleSingleDownload(SocketType sock, const std::string& model_id);
        
        // Handle all downloads status request (GET)
        void handleAllDownloads(SocketType sock);
        
        // Handle cancel single download (DELETE/POST)
        void handleCancelDownload(SocketType sock, const std::string& model_id);
        
        // Handle cancel all downloads (DELETE/POST)
        void handleCancelAllDownloads(SocketType sock);
        
        // Handle pause download (POST)
        void handlePauseDownload(SocketType sock, const std::string& model_id);
        
        // Handle resume download (POST)
        void handleResumeDownload(SocketType sock, const std::string& model_id);
    };

} // namespace kolosal

#endif // KOLOSAL_DOWNLOADS_ROUTE_HPP
