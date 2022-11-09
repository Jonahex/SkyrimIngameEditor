#include "Gui/MainWindow.h"

#include "Gui/CellEditor.h"
#include "Gui/TargetEditor.h"
#include "Gui/Utils.h"
#include "Gui/WeatherEditor.h"
#include "Serialization/Serializer.h"
#include "Utils/Engine.h"
#include "Utils/TargetManager.h"

#include <RE/M/Main.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/S/Sky.h>
#include <RE/T/TESGlobal.h>

#include <imgui.h>

namespace SIE
{
	namespace SMainWindow
	{
		bool VisibilityFlagEdit(const char* label, RE::VISIBILITY flagMask) 
		{
			static const REL::Relocation<std::uint32_t*> visibilityFlag(RE::Offset::VisibilityFlag);

			bool isVisible = !((*visibilityFlag) & (1 << static_cast<std::uint32_t>(flagMask)));
			const bool wasEdited = ImGui::Checkbox(label, &isVisible);
			if (wasEdited)
			{
				SetVisibility(flagMask, isVisible);
			}
			return wasEdited;
		}

		bool ImageSpaceEOFFlagEdit(const char* label) 
		{
			static const REL::Relocation<bool*> isEnabledFlag(
				RE::Offset::ImageSpaceEOFEffectsEnabledFlag);

			return ImGui::Checkbox(label, isEnabledFlag.get());
		}

		bool VolumetricLightingFlagEdit(const char* label)
		{
			static const REL::Relocation<bool*> isEnabledFlag(
				RE::Offset::VolumetricLightingEnabledFlag);
			static const REL::Relocation<bool*> unkFlag(RELOCATION_ID(528190, 415135));
			static const REL::Relocation<bool***> unkObjbect(RELOCATION_ID(527731, 414660));

			const bool wasEdited = ImGui::Checkbox(label, isEnabledFlag.get());
			if (wasEdited)
			{
				*(*(*unkObjbect + 46) + 8) = *isEnabledFlag;
				*unkFlag = false;
			}
			return wasEdited;
		}
	}

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

		if (PushingCollapsingHeader("Visibility"))
		{
			if (PushingCollapsingHeader("Objects"))
			{
				SMainWindow::VisibilityFlagEdit("Actor", RE::VISIBILITY::kActor);
				SMainWindow::VisibilityFlagEdit("Marker", RE::VISIBILITY::kMarker);
				SMainWindow::VisibilityFlagEdit("Land", RE::VISIBILITY::kLand);
				SMainWindow::VisibilityFlagEdit("Static", RE::VISIBILITY::kStatic);
				SMainWindow::VisibilityFlagEdit("Dynamic", RE::VISIBILITY::kDynamic);
				SMainWindow::VisibilityFlagEdit("MultiBound", RE::VISIBILITY::kMultiBound);
				SMainWindow::VisibilityFlagEdit("Water", RE::VISIBILITY::kWater);

				ImGui::TreePop();
			}

			if (PushingCollapsingHeader("Effects"))
			{
				SMainWindow::VolumetricLightingFlagEdit("Volumetric Lighting");
				SMainWindow::ImageSpaceEOFFlagEdit("End of Frame ImageSpaceEffect");

				auto& targetManager = TargetManager::Instance();
				bool enableHighlight = targetManager.GetEnableTargetHighlight();
				if (ImGui::Checkbox("Target Highlight", &enableHighlight))
				{
					targetManager.SetEnableTargetHightlight(enableHighlight);
				}

				ImGui::TreePop();
			}

			ImGui::TreePop();
		}

		if (PushingCollapsingHeader("Time"))
		{
			const auto main = RE::Main::GetSingleton();
			ImGui::Checkbox("Freeze", &main->freezeTime);
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
