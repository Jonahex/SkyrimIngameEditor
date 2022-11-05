#pragma once

#include <unordered_set>

namespace RE
{
	class TESWeather;
}

namespace Hooks
{
	inline std::unordered_set<RE::TESWeather*> Weathers;

	void Install();

	void OnPostPostLoad();
}
