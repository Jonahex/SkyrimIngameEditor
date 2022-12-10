#pragma once

#include "Systems/EngineSystem.h"

namespace SIE
{
	class CameraSystem : public EngineSystem
	{
	public:
		CameraSystem();

		void HandleInput(UINT Msg, WPARAM wParam, LPARAM lParam) override;
	};
}
