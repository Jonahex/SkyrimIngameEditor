#pragma once

#include <chrono>

#include <Windows.h>

namespace SIE
{
	class EngineSystem
	{
	public:
		virtual ~EngineSystem() = default;

		virtual void Process(std::chrono::microseconds deltaTime){};
		virtual void HandleInput(UINT Msg, WPARAM wParam, LPARAM lParam){};
	};
}
