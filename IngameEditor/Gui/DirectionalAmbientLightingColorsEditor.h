#pragma once

#include <RE/B/BGSDirectionalAmbientLightingColors.h>

namespace SIE
{
	bool DirectionalAmbientLightingColorsDirectionalEditor(const char* label,
		RE::BGSDirectionalAmbientLightingColors::Directional& directional);
	bool DirectionalAmbientLightingColorsEditor(const char* label,
		RE::BGSDirectionalAmbientLightingColors& colors);
}
