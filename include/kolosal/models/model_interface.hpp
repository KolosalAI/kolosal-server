#pragma once

#include "../export.hpp"

#include <json.hpp>

class KOLOSAL_SERVER_API IModel {
public:
	virtual bool validate() const = 0;
	virtual void from_json(const nlohmann::json& j) = 0;
	virtual nlohmann::json to_json() const = 0;
	virtual ~IModel() {}
};
