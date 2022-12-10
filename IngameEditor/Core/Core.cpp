#include "Core/Core.h"

#include "Systems/CameraSystem.h"
#include "Systems/WeatherEditorSystem.h"

namespace SIE
{
	Core::Core() 
	{ 
		systems[&typeid(CameraSystem)] = std::make_unique<CameraSystem>();
		systems[&typeid(WeatherEditorSystem)] = std::make_unique<WeatherEditorSystem>();
	}

	void Core::Process(std::chrono::microseconds deltaTime) 
	{
		for (const auto& [id, system] : systems)
		{
			system->Process(deltaTime);
		}
	}

	void Core::HandleInput(UINT Msg, WPARAM wParam, LPARAM lParam) 
	{
		for (const auto& [id, system] : systems)
		{
			system->HandleInput(Msg, wParam, lParam);
		}
	}
}
