#pragma once

#include <nlohmann/json.hpp>

namespace RE
{
	class TESForm;

	void to_json(nlohmann::json& j, const TESForm& form);
}
