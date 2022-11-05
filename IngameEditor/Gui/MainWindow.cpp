#include "Gui/MainWindow.h"

#include "Gui/CellEditor.h"
#include "Gui/TargetEditor.h"
#include "Gui/Utils.h"
#include "Gui/WeatherEditor.h"
#include "Serialization/Serializer.h"

#include <RE/S/Sky.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/T/TESGlobal.h>

#include <imgui.h>

namespace SIE
{
	void MainWindow()
	{
		ImGui::Begin("Ingame Editor");

		{
			static RE::BSFixedString savePath = "WeatherEditorOutput.esp";
			EspPathEdit("SavePath", "", savePath);
			ImGui::SameLine();
			if (ImGui::Button("Save"))
			{
				Serializer::Instance().Export(std::string("Data\\") + savePath.c_str());
			}
		}

		if (PushingCollapsingHeader("Time"))
		{
			if (RE::TESGlobal* gameHour = FindGlobal("GameHour"))
			{
				ImGui::SliderFloat("GameHour", &gameHour->value, 0.f, 24.f);
			}
			if (RE::TESGlobal* timeScale = FindGlobal("TimeScale"))
			{
				ImGui::DragFloat("TimeScale", &timeScale->value, 0.1f, 0.f, 100.f);
			}
			ImGui::TreePop();
		}

		RE::Sky* sky = RE::Sky::GetSingleton();
		if (sky != nullptr)
		{
			RE::TESWeather* currentWeather = sky->currentWeather;
			if (currentWeather != nullptr)
			{
				if (PushingCollapsingHeader("Weather Editor"))
				{
					if (FormSelector<false>("Current weather", currentWeather))
					{
						sky->ForceWeather(currentWeather, true);
					}
					WeatherEditor("WeatherEditor", *currentWeather);
					ImGui::TreePop();
				}
			}
		}

		if (PushingCollapsingHeader("Target Editor"))
		{
			TargetEditor("TargetEditor");
			ImGui::TreePop();
		}

		const auto player = RE::PlayerCharacter::GetSingleton();
		if (const auto currentCell = player->GetParentCell())
		{
			if (PushingCollapsingHeader("Cell Editor"))
			{
				CellEditor("CellEditor", *currentCell);
				ImGui::TreePop();
			}
		}

		ImGui::End();
	}
}
