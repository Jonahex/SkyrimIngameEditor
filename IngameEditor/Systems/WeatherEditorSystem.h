#pragma once

#include "Systems/EngineSystem.h"

namespace RE
{
	class TESWeather;
}

namespace SIE
{
	class WeatherEditorSystem : public EngineSystem
	{
	public:
		void Process(std::chrono::microseconds deltaTime) override;

		bool IsWeatherLocked() const;
		void LockWeather(bool lock);

		void SetWeather(RE::TESWeather* weatherToSet);

	private:
		RE::TESWeather* weatherToLock = nullptr;
	};
}
