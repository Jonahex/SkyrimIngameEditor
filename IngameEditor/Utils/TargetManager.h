#pragma once

#include <RE/N/NiSmartPointer.h>

namespace RE
{
	class TESObjectREFR;
}

namespace SIE
{
	class TargetManager
	{
	public:
		static TargetManager& Instance();

		RE::TESObjectREFR* GetTarget() const;
		void TrySetTargetAt(int screenX, int screenY);

		bool GetEnableTargetHighlight() const;
		void SetEnableTargetHightlight(bool value);

	private:
		TargetManager() = default;

		RE::NiPointer<RE::TESObjectREFR> target;
		bool enableTargetHighlight = true;
	};
}
