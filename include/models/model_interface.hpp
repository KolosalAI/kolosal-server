#ifndef IMODEL_HPP
#define IMODEL_HPP

#include <json.hpp>

class IModel {
public:
	virtual bool validate() const = 0;
	virtual void from_json(const nlohmann::json& j) = 0;
	virtual nlohmann::json to_json() const = 0;
	virtual ~IModel() {}
};

#endif  // IMODEL_HPP
