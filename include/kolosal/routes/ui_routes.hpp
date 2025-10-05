#ifndef KOLOSAL_UI_ROUTES_HPP
#define KOLOSAL_UI_ROUTES_HPP

#include "route_interface.hpp"

namespace kolosal {

    class UIRoute : public IRoute {
    public:
        bool match(const std::string& method, const std::string& path) override;
        void handle(SocketType sock, const std::string& body) override;
    
    private:
        std::string current_method_;
        std::string current_path_;
        
        // Helper methods
        bool isStaticAsset(const std::string& path);
        std::string getContentType(const std::string& filePath);
        std::string readStaticFile(const std::string& relativePath);
        void serveStaticFile(SocketType sock, const std::string& filePath, const std::string& contentType);
        void serve404(SocketType sock);
    };

} // namespace kolosal

#endif // KOLOSAL_UI_ROUTES_HPP