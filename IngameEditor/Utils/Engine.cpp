#include "Utils/Engine.h"

#include <RE/T/TES.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESGlobal.h>
#include <RE/T/TESWaterSystem.h>

#include <magic_enum.hpp>

namespace SIE
{
	float U8ToFloatColor(uint8_t value)
	{
	    return std::min(static_cast<float>(value) / 255.f, 1.f);
	}

	uint8_t FloatToU8Color(float value)
	{
	    return static_cast<uint8_t>(std::min(255.f, 255.f * value));
	}

	std::string GetFullName(const RE::TESForm& form)
	{
		return std::format("{} [{:X}]", form.GetFormEditorID(), form.GetFormID());
	}

	std::string GetTypedName(const RE::TESForm& form)
	{
		return std::format("<{}> {} [{:X}]", magic_enum::enum_name(form.formType.get()), form.GetFormEditorID(), form.GetFormID());
	}

	RE::TESGlobal* FindGlobal(const std::string& editorId)
	{
		const auto dataHandler = RE::TESDataHandler::GetSingleton();
		const auto& globals = dataHandler->GetFormArray<RE::TESGlobal>();
		for (const auto& global : globals)
		{
			if (global->GetFormEditorID() == editorId)
			{
				return global;
			}
		}
		return nullptr;
	}

	void ResetTimeTo(float time)
	{
		const auto gameHour = FindGlobal("GameHour");
		gameHour->value = time;
	}

	void ResetTimeTo(RE::TESWeather::ColorTimes::ColorTime colorTime)
	{
		switch (colorTime)
		{
		case RE::TESWeather::ColorTime::kSunrise:
			ResetTimeTo(6.0f);
			break;
		case RE::TESWeather::ColorTime::kDay:
			ResetTimeTo(12.0f);
			break;
		case RE::TESWeather::ColorTime::kSunset:
			ResetTimeTo(18.0f);
			break;
		case RE::TESWeather::ColorTime::kNight:
			ResetTimeTo(0.0f);
			break;
		default:;
		}
	}

	void SetVisibility(RE::VISIBILITY visibility, bool isVisible)
	{
		RE::TES::GetSingleton()->SetVisibility(visibility, isVisible, false);
		if (visibility == RE::VISIBILITY::kWater)
		{
			RE::TESWaterSystem::GetSingleton()->SetVisibility(isVisible, false, false);
		}
	}
}
