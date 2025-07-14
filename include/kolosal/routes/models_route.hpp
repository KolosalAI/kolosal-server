#ifndef KOLOSAL_ROUTES_MODELS_ROUTE_HPP
#define KOLOSAL_ROUTES_MODELS_ROUTE_HPP

#include "route_interface.hpp"
#include <string>
#include <regex>

namespace kolosal
{
    /**
     * @brief Unified route for all model management operations
     * 
     * Handles the following endpoints:
     * - GET /models, /v1/models - List all models
     * - POST /models, /v1/models - Add a new model
     * - GET /models/{id}, /v1/models/{id} - Get model status
     * - DELETE /models/{id}, /v1/models/{id} - Remove a model
     * - GET /models/{id}/status, /v1/models/{id}/status - Get detailed model status
     */
    class ModelsRoute : public IRoute
    {
    public:
        bool match(const std::string &method, const std::string &path) override;
        void handle(SocketType sock, const std::string &body) override;

    private:
        // Store the matched path and method to determine operation in handle()
        mutable std::string matched_path_;
        mutable std::string matched_method_;
        
        // Handler methods for different operations
        void handleListModels(SocketType sock, const std::string &body);
        void handleAddModel(SocketType sock, const std::string &body);
        void handleGetModel(SocketType sock, const std::string &body, const std::string &modelId);
        void handleRemoveModel(SocketType sock, const std::string &body, const std::string &modelId);
        void handleModelStatus(SocketType sock, const std::string &body, const std::string &modelId);

        // Helper methods
        std::string extractModelIdFromPath(const std::string &path);
        bool isStatusEndpoint(const std::string &path);
        
        // Regex patterns for path matching
        static const std::regex modelsPattern_;
        static const std::regex modelIdPattern_;
        static const std::regex modelStatusPattern_;
    };

} // namespace kolosal

#endif // KOLOSAL_ROUTES_MODELS_ROUTE_HPP
