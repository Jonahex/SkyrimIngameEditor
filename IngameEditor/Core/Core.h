#pragma once

#include "Systems/EngineSystem.h"

namespace SIE
{
	class Core
	{
	public:
		static Core& GetInstance()
		{ 
			static Core instance;
			return instance;
		}

		template<typename T>
		T* GetSystem()
		{
			const auto& typeInfo = typeid(T);

			auto it = systems.find(&typeInfo);
			if (it != systems.cend())
			{
				return static_cast<T*>(it->second.get());
			}

			return nullptr;
		}

		void Process(std::chrono::microseconds deltaTime);
		void HandleInput(UINT Msg, WPARAM wParam, LPARAM lParam);

	private:
		Core();

		std::unordered_map<const std::type_info*, std::unique_ptr<EngineSystem>> systems;
	};
}
