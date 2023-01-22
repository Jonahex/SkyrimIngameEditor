#include "Gui/MainWindow.h"

#include "Core/ShaderCache.h"
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

#include <RE/B/BGSGrassManager.h>
#include <RE/B/BSRenderPass.h>
#include <RE/I/ImageSpaceEffect.h>
#include <RE/I/ImageSpaceEffectManager.h>
#include <RE/M/Main.h>
#include <RE/N/NiCamera.h>
#include <RE/N/NiRTTI.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/S/Sky.h>
#include <RE/T/TES.h>
#include <RE/T/TESGlobal.h>
#include <RE/T/TESObjectCELL.h>

#include <imgui.h>

namespace SIE
{
	namespace SMainWindow
	{
		struct RenderPassCache
		{
			struct Pool
			{
				RE::BSRenderPass* passes;
				RE::BSLight** lights;
				RE::BSRenderPass* firstAvailable;
				RE::BSRenderPass* lastAvailable;
				RE::BSSpinLock lock;
			};

			static constexpr size_t PoolCount = 2;

			std::array<Pool, PoolCount> pools;
		};

		void RenderPassStatistics() 
		{
			static const REL::Relocation<RenderPassCache*> renderPassCache(
				RELOCATION_ID(528206, 415150));

			for (size_t poolIndex = 0; poolIndex < RenderPassCache::PoolCount; ++poolIndex)
			{
				const auto& pool = (*renderPassCache).pools[poolIndex];

				std::array<size_t, static_cast<size_t>(RE::BSShader::Type::Total) + 1> usedPasses;
				std::fill(usedPasses.begin(), usedPasses.end(), 0);

				constexpr size_t passCount = 65535;
				for (size_t passIndex = 0; passIndex < passCount; ++passIndex)
				{
					const RE::BSRenderPass& pass = pool.passes[passIndex];
					if (pass.passEnum != 0)
					{
						/*logger::info("{} {} {}", pass.passEnum,
							reinterpret_cast<void*>(pass.shaderProperty),
							reinterpret_cast<void*>(pass.shader));*/
						++usedPasses[static_cast<size_t>(RE::BSShader::Type::Total)];
						if (pass.shaderProperty != nullptr)
						{
							++usedPasses[static_cast<size_t>(pass.shader->shaderType.get())];
						}
					}
				}

				ImGui::Text("%d/%d passes used in pool %d including:",
					usedPasses[static_cast<size_t>(RE::BSShader::Type::Total)], passCount,
					poolIndex);
				for (size_t typeIndex = 0;
					 typeIndex < static_cast<size_t>(RE::BSShader::Type::Total); ++typeIndex)
				{
					if (usedPasses[typeIndex] > 0)
					{
						ImGui::Text("%d %s passes", usedPasses[typeIndex],
							magic_enum::enum_name(static_cast<RE::BSShader::Type>(typeIndex))
								.data());
					}
				}
			}
		}

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

		bool GrassVisibilityEdit(const char* label) 
		{
			bool isVisible = IsGrassVisible();
			const bool wasEdited = ImGui::Checkbox(label, &isVisible);
			if (wasEdited)
			{
				SetGrassVisible(isVisible);
			}
			return wasEdited;
		}

		void UpdateGrassFadeDistance() 
		{
			static const REL::Relocation<void(float, float, float, float)> updateFunc{ RELOCATION_ID(99914, 106557)
			};
			static const REL::Relocation<float*> startFadeDistance(RELOCATION_ID(501109, 359414));
			static const REL::Relocation<float*> fadeRange(RELOCATION_ID(501111, 359417));
			auto grassManager = RE::BGSGrassManager::GetSingleton();

			const auto newDistance = *startFadeDistance + *fadeRange;
			grassManager->fadeDistance = *startFadeDistance + *fadeRange;
			updateFunc(0.f, 0.f, *startFadeDistance, grassManager->fadeDistance);
		}

		bool GrassFadeDistanceEdit(const char* label) 
		{
			static const REL::Relocation<float*> startFadeDistance(RELOCATION_ID(501109, 359414));

			const bool wasEdited = ImGui::DragFloat(label, &*startFadeDistance, 100.f, 0.f);
			if (wasEdited)
			{
				UpdateGrassFadeDistance();
			}
			return wasEdited;
		}

		bool NiAVObjectVisibilityEdit(const char* label, RE::NiAVObject* object) 
		{
			if (object == nullptr)
			{
				return false;
			}

			bool isVisible = !object->flags.any(RE::NiAVObject::Flag::kHidden);
			const bool wasEdited = ImGui::Checkbox(label, &isVisible);
			if (wasEdited)
			{
				if (isVisible)
				{
					object->flags.reset(RE::NiAVObject::Flag::kHidden);
				}
				else
				{
					object->flags.set(RE::NiAVObject::Flag::kHidden);
				}
			}
			return wasEdited;
		}
	}

	void MainWindow()
	{
		ImGui::SetNextWindowSize(ImVec2(512.f, 1024.f),
			ImGuiCond_FirstUseEver);
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

				SMainWindow::GrassVisibilityEdit("Grass");
				ImGui::SameLine();
				SMainWindow::GrassFadeDistanceEdit("Fade Distance");

				static const REL::Relocation<RE::NiNode**> treeLODNode(RELOCATION_ID(516170, 402321));
				static const REL::Relocation<RE::NiNode**> waterLODNode(
					RELOCATION_ID(516171, 402322));
				static const REL::Relocation<RE::NiNode**> landLODNode(
					RELOCATION_ID(516172, 402324));
				static const REL::Relocation<RE::NiNode**> objectsLODNode(
					RELOCATION_ID(516173, 402325));
				static const REL::Relocation<RE::NiNode**> cloudsLODNode(
					RELOCATION_ID(516174, 406687));

				SMainWindow::NiAVObjectVisibilityEdit("Tree LOD", *treeLODNode);
				SMainWindow::NiAVObjectVisibilityEdit("Water LOD", *waterLODNode);
				SMainWindow::NiAVObjectVisibilityEdit("Land LOD", *landLODNode);
				SMainWindow::NiAVObjectVisibilityEdit("Objects LOD", *objectsLODNode);
				SMainWindow::NiAVObjectVisibilityEdit("Clouds LOD", *cloudsLODNode);

				ImGui::TreePop();
			}

			if (PushingCollapsingHeader("Effects"))
			{
				auto isem = RE::ImageSpaceEffectManager::GetSingleton();

				/*if (PushingCollapsingHeader("Imagespace Shaders"))
				{
					ImGui::Checkbox("Apply Reflections",
						&isem->shaderInfo.applyReflectionsShaderInfo->isEnabled);
					ImGui::Checkbox("Apply Volumetric Lighting",
						&isem->shaderInfo.applyVolumetricLightingShaderInfo->isEnabled);
					ImGui::Checkbox("Basic Copy", &isem->shaderInfo.basicCopyShaderInfo->isEnabled);
					ImGui::Checkbox("Lens Flares", &isem->shaderInfo.iblfShaderInfo->isEnabled);
					ImGui::Checkbox("Blur CS", &isem->shaderInfo.blurCSShaderInfo->isEnabled);
					ImGui::Checkbox("Composite Lens Flare Volumetric Lighting",
						&isem->shaderInfo.compositeLensFlareVolumetricLightingShaderInfo
							 ->isEnabled);
					ImGui::Checkbox("Copy Subregion CS",
						&isem->shaderInfo.copySubRegionCSShaderInfo->isEnabled);
					ImGui::Checkbox("Debug Snow", &isem->shaderInfo.debugSnowShaderInfo->isEnabled);
					ImGui::Checkbox("Exponential Prefilter",
						&isem->shaderInfo.ibExponentialPreFilterShaderInfo->isEnabled);
					ImGui::Checkbox("Lighting Composite",
						&isem->shaderInfo.lightingCompositeShaderInfo->isEnabled);
					ImGui::Checkbox("Perlin Noise CS",
						&isem->shaderInfo.perlinNoiseCSShaderInfo->isEnabled);
					ImGui::Checkbox("Reflections",
						&isem->shaderInfo.reflectionsShaderInfo->isEnabled);
					ImGui::Checkbox("Scalable Ambient Obscurance",
						&isem->shaderInfo.scalableAmbientObscuranceShaderInfo->isEnabled);
					ImGui::Checkbox("Scalable Ambient Obscurance CS",
						&isem->shaderInfo.saoCSShaderInfo->isEnabled);
					ImGui::Checkbox("Indirect Lighting",
						&isem->shaderInfo.indirectLightingShaderInfo->isEnabled);
					ImGui::Checkbox("Simple Color",
						&isem->shaderInfo.simpleColorShaderInfo->isEnabled);
					ImGui::Checkbox("Snow SSS", &isem->shaderInfo.snowSSSShaderInfo->isEnabled);
					ImGui::Checkbox("Temporal AAA",
						&isem->shaderInfo.temporalAAShaderInfo->isEnabled);
					ImGui::Checkbox("Upsample Dynamic Resolution",
						&isem->shaderInfo.upsampleDynamicResolutionShaderInfo->isEnabled);
					ImGui::Checkbox("Water Blend",
						&isem->shaderInfo.waterBlendShaderInfo->isEnabled);
					ImGui::Checkbox("Underwater Mask",
						&isem->shaderInfo.underwaterMaskShaderInfo->isEnabled);

					ImGui::TreePop();
				}*/

				ImGui::Checkbox("HDR", &*REL::Relocation<bool*>(RE::Offset::HDREnabledFlag));
				ImGui::Checkbox("Fog",
					&*REL::Relocation<bool*>(RE::Offset::SAOApplyFogEnabledFlag));
				ImGui::Checkbox("Volumetric Lighting",
					&*REL::Relocation<bool*>(
						RE::Offset::Texture3dAccumulationVolumetricLightingEnabledFlag));
				ImGui::Checkbox("End of Frame ImageSpaceEffect",
					&*REL::Relocation<bool*>(
						RE::Offset::ImageSpaceEOFEffectsEnabledFlag));

				ImGui::TreePop();
			}

			if (PushingCollapsingHeader("Editor"))
			{
				auto& shaderCache = ShaderCache::Instance();
				bool useCustomShaders = shaderCache.IsEnabled();
				if (ImGui::Checkbox("Use Custom Shaders", &useCustomShaders))
				{
					shaderCache.SetEnabled(useCustomShaders);
				}
				if (useCustomShaders)
				{
					for (size_t classIndex = 0;
						 classIndex < static_cast<size_t>(ShaderClass::Total); ++classIndex)
					{
						const auto shaderClass = static_cast<ShaderClass>(classIndex);
						bool useCustomClassShaders = shaderCache.IsEnabledForClass(shaderClass);
						if (ImGui::Checkbox(std::format("Use Custom {} Shaders",
												magic_enum::enum_name(shaderClass))
												.c_str(),
								&useCustomClassShaders))
						{
							shaderCache.SetEnabledForClass(shaderClass, useCustomClassShaders);
						}
					}
				}

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

		if (PushingCollapsingHeader("Statistics"))
		{
			if (PushingCollapsingHeader("Render passes"))
			{
				SMainWindow::RenderPassStatistics();
				ImGui::TreePop();
			}
			ImGui::TreePop();
		}

		if (PushingCollapsingHeader("Misc"))
		{
			if (ImGui::Button("Clear custom shader cache"))
			{
				auto& shaderCache = ShaderCache::Instance();
				shaderCache.Clear();
			}
			if (ImGui::Button("Enable all"))
			{
				const auto tes = RE::TES::GetSingleton();
				tes->ForEachReference(
					[](RE::TESObjectREFR& refr)
					{
						if (refr.IsDisabled())
						{
							refr.Enable();
						}
						return RE::BSContainer::ForEachResult::kContinue;
					});
			}
			ImGui::TreePop();
		}

		ImGui::End();
	}
}
