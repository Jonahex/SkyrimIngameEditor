#include "Systems/WeatherEditorSystem.h"

#include <RE/S/Sky.h>

namespace SIE
{
	void WeatherEditorSystem::Process(std::chrono::microseconds deltaTime) 
	{ 
		if (weatherToLock != nullptr)
		{
			if (auto sky = RE::Sky::GetSingleton(); sky != nullptr && sky->currentWeather != weatherToLock)
			{
				sky->ForceWeather(weatherToLock, true);
			}
		}
	}

	bool WeatherEditorSystem::IsWeatherLocked() const 
	{ 
		return weatherToLock != nullptr; 
	}

	void WeatherEditorSystem::LockWeather(bool lock) 
	{ 
		if (!lock)
		{
			weatherToLock = nullptr;
		}
		else if (auto sky = RE::Sky::GetSingleton())
		{
			weatherToLock = sky->currentWeather;
		}
	}

	void WeatherEditorSystem::SetWeather(RE::TESWeather* weatherToSet) 
	{
		if (auto sky = RE::Sky::GetSingleton())
		{
			if (weatherToLock != nullptr)
			{
				weatherToLock = weatherToSet;
			}
			sky->ForceWeather(weatherToSet, true);
		}
	}
}
