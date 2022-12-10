#pragma once

#include <RE/T/TESDataHandler.h>
#include <RE/T/TESWeather.h>
#include <RE/V/Visibility.h>

namespace RE
{
	class NiAVObject;
	class TESWaterForm;
}

namespace SIE
{
	float U8ToFloatColor(uint8_t value);
	uint8_t FloatToU8Color(float value);
	std::string GetFullName(const RE::TESForm& form);
	std::string GetTypedName(const RE::TESForm& form);
	RE::TESGlobal* FindGlobal(const std::string& editorId);
	void ResetTimeTo(float time);
	void ResetTimeTo(RE::TESWeather::ColorTimes::ColorTime colorTime);
	void SetVisibility(RE::VISIBILITY visibility, bool isVisible);
	void UpdateWaterGeometry(RE::NiAVObject* geometry, RE::TESWaterForm* waterType);
}
