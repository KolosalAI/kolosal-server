#ifndef KOLOSAL_LIST_INFERENCE_ENGINES_ROUTE_HPP
#define KOLOSAL_LIST_INFERENCE_ENGINES_ROUTE_HPP

#include "route_interface.hpp"

namespace kolosal
{

    /**
     * @brief Route handler for listing available inference engine libraries
     * 
     * Handles GET requests to /inference-engines and /v1/inference-engines
     * Returns a list of all available inference engine libraries that can be loaded
     */
    class ListInferenceEnginesRoute : public IRoute
    {
    public:
        /**
         * @brief Check if this route matches the given request
         * @param method HTTP method (should be GET)
         * @param path Request path (should be /inference-engines or /v1/inference-engines)
         * @return True if this route should handle the request
         */
        bool match(const std::string &method, const std::string &path) override;

        /**
         * @brief Handle the list inference engines request
         * @param sock Socket to send response to
         * @param body Request body (ignored for GET requests)
         */
        void handle(SocketType sock, const std::string &body) override;
    };

} // namespace kolosal

#endif // KOLOSAL_LIST_INFERENCE_ENGINES_ROUTE_HPP
