#pragma once

namespace RE
{
	class NiPoint3;
	class NiTransform;
	class TESObjectREFR;
}

namespace SIE
{
	bool NiPoint3Editor(const char* label, RE::NiPoint3& vector, float speed = 1.f);
	bool NiTransformEditor(const char* label, RE::NiTransform& transform,
		const RE::NiTransform& parentTransform = {});
	bool ReferenceTransformEditor(const char* label, RE::TESObjectREFR& ref);
}
