#pragma once

#include <RE/T/TESDataHandler.h>
#include <RE/T/TESWeather.h>
#include <RE/V/Visibility.h>

namespace RE
{
	class NiAVObject;
	class NiObject;
	class Sky;
	class TESObjectCELL;
	class TESWaterForm;
}

namespace SIE
{
	float U8ToFloatColor(uint8_t value);
	uint8_t FloatToU8Color(float value);
	std::string GetCellFullName(const RE::TESObjectCELL& form);
	std::string GetFullName(const RE::TESForm& form);
	std::string GetTypedName(const RE::TESForm& form);
	RE::TESGlobal* FindGlobal(const std::string& editorId);
	void ResetTimeTo(float time);
	void ResetTimeForFog(RE::Sky& sky, bool isDay);
	void ResetTimeForColor(RE::Sky& sky, RE::TESWeather::ColorTimes::ColorTime colorTime);
	void SetVisibility(RE::VISIBILITY visibility, bool isVisible);
	void UpdateWaterGeometry(RE::NiAVObject* geometry, RE::TESWaterForm* waterType);
	void ReloadWaterObjects();

	bool IsGrassVisible();
	void SetGrassVisible(bool value);

	std::string GetFullName(const RE::NiObject& object);
}
