#include "Gui/MainWindow.h"

#include "Gui/CellEditor.h"
#include "Gui/Gui.h"
#include "Gui/TargetEditor.h"
#include "Gui/Utils.h"
#include "Gui/WeatherEditor.h"
#include "Serialization/Serializer.h"
#include "Systems/WeatherEditorSystem.h"
#include "Utils/Engine.h"
#include "Utils/OverheadBuilder.h"
#include "Utils/TargetManager.h"

#include <RE/I/ImageSpaceEffectManager.h>
#include <RE/M/Main.h>
#include <RE/N/NiCamera.h>
#include <RE/N/NiRTTI.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/S/Sky.h>
#include <RE/T/TESGlobal.h>
#include <RE/T/TESObjectCELL.h>

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

			const bool wasEdited = ImGui::Checkbox(label, isEnabledFlag.get());
			if (wasEdited)
			{
				RE::ImageSpaceEffectManager::GetSingleton()
					->shaderInfo.applyVolumetricLightingShaderInfo->isEnabled = *isEnabledFlag;
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

		auto& core = Core::GetInstance();
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
						if (auto weatherEditorSystem = core.GetSystem<WeatherEditorSystem>())
						{
							weatherEditorSystem->SetWeather(currentWeather);
						}
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

			if (currentCell->IsExteriorCell())
			{
#ifdef OVERHEAD_TOOL
				if (PushingCollapsingHeader("Overhead Builder"))
				{
					ImGui::DragScalarN("Min", ImGuiDataType_S16, &OverheadBuilder::Instance().minX,
						2);
					ImGui::DragScalarN("Max", ImGuiDataType_S16, &OverheadBuilder::Instance().maxX,
						2);
					if (ImGui::Button("Build"))
					{
						OverheadBuilder::Instance().Start();
						Gui::Instance().SetEnabled(false);
					}
					ImGui::SameLine();
					if (ImGui::Button("Stop"))
					{
						OverheadBuilder::Instance().Finish();
					}
					ImGui::TreePop();
				}
#endif
			}
		}

#ifdef OVERHEAD_TOOL
		if (const auto playerCamera = RE::PlayerCamera::GetSingleton())
		{
			if (PushingCollapsingHeader("Camera"))
			{
				if (const auto root = playerCamera->cameraRoot.get();
					root != nullptr && root->children.size() > 0 &&
					std::strcmp(root->children[0]->GetRTTI()->name, "NiCamera") == 0)
				{
					auto camera = static_cast<RE::NiCamera*>(root->children[0].get());
					if (PushingCollapsingHeader("Frustum"))
					{
						/*ImGui::Checkbox("Orthographic", &camera->viewFrustum.bOrtho);
						ImGui::DragFloat("Left", &camera->viewFrustum.fLeft);
						ImGui::DragFloat("Right", &camera->viewFrustum.fRight);
						ImGui::DragFloat("Top", &camera->viewFrustum.fTop);
						ImGui::DragFloat("Bottom", &camera->viewFrustum.fBottom);
						ImGui::DragFloat("Near", &camera->viewFrustum.fNear);
						ImGui::DragFloat("Far", &camera->viewFrustum.fFar);*/
						ImGui::Checkbox("Orthographic", &OverheadBuilder::Instance().ortho);
						ImGui::DragFloat("Left", &OverheadBuilder::Instance().left);
						ImGui::DragFloat("Right", &OverheadBuilder::Instance().right);
						ImGui::DragFloat("Top", &OverheadBuilder::Instance().top);
						ImGui::DragFloat("Bottom", &OverheadBuilder::Instance().bottom);
						ImGui::DragFloat("Near", &OverheadBuilder::Instance().nearPlane);
						ImGui::DragFloat("Far", &OverheadBuilder::Instance().farPlane);
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
		}
#endif

		ImGui::End();
	}
}
