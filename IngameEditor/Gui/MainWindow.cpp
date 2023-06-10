#include "Gui/MainWindow.h"

#include "Core/ShaderCache.h"
#include "Gui/CellEditor.h"
#include "Gui/Gui.h"
#include "Gui/NiObjectEditor.h"
#include "Gui/TargetEditor.h"
#include "Gui/Utils.h"
#include "Gui/WeatherEditor.h"
#include "Serialization/Serializer.h"
#include "Systems/WeatherEditorSystem.h"
#include "Utils/Engine.h"
#include "Utils/OverheadBuilder.h"
#include "Utils/RTTICache.h"
#include "Utils/TargetManager.h"

#include <RE/B/BGSGrassManager.h>
#include <RE/B/BSRenderPass.h>
#include <RE/C/CombatBehaviorTree.h>
#include <RE/C/CombatBehaviorTreeNode.h>
#include <RE/C/CombatBehaviorTreeRootNode.h>
#include <RE/I/ImageSpaceEffect.h>
#include <RE/M/Main.h>
#include <RE/N/NiCamera.h>
#include <RE/N/NiNode.h>
#include <RE/N/NiRTTI.h>
#include <RE/N/NiStream.h>
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

		void NifEditor(const char* label, RE::NiNode& rootNode) 
		{ 
			ImGui::PushID(label);

			static RE::NiPointer<RE::NiNode> editObject;

			if (editObject != nullptr)
			{
				if (ImGui::Button("Clear"))
				{
					rootNode.DetachChild(editObject.get());
					editObject.reset();
				}
			}

			static RE::BSFixedString loadPath;
			FreeMeshPathEdit("LoadPath", "", loadPath);
			ImGui::SameLine();
			if (ImGui::Button("Load"))
			{
				auto stream = RE::NiStream::Create();
				stream->Load3(loadPath.c_str());
				if (!stream->topObjects.empty())
				{
					if (auto avObject =
							netimmerse_cast<RE::NiNode*>(stream->topObjects.front().get()))
					{
						if (editObject != nullptr)
						{
							rootNode.DetachChild(editObject.get());
							editObject.reset();
						}
						editObject = RE::NiPointer(static_cast<RE::NiNode*>(RTTICache::Instance().Construct(
							*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiNode))));
						editObject->name = "Nif Editor Object";
						editObject->AttachChild(avObject);
						editObject->local.translate = { 0.f, 50.f, 110.f };
						//editObject = RE::NiPointer(avObject);
						rootNode.AttachChild(editObject.get());
					}
				}
				stream->~NiStream();
				RE::free(stream);
			}

			if (editObject != nullptr)
			{
				if (DispatchableNiObjectEditor("", *editObject))
				{
					RE::NiUpdateData updateData;
					updateData.flags.set(RE::NiUpdateData::Flag::kDirty);
					editObject->Update(updateData);
				}
			}

			ImGui::PopID();
		}

		void CombatBehaviorTreeNodeViewer(const RE::CombatBehaviorTreeNode& node) 
		{ 
			const auto& typeName = RTTICache::Instance().GetTypeName(&node);
			if (PushingCollapsingHeader(std::format("{} [{}]", node.GetName(), typeName).c_str()))
			{
				for (const auto subNode : node.children)
				{
					if (subNode != nullptr)
					{
						CombatBehaviorTreeNodeViewer(*subNode);
					}
				}

				ImGui::TreePop();
			}
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
				bool isAsync = shaderCache.IsAsync();
				if (ImGui::Checkbox("Async Shaders Loading", &isAsync))
				{
					shaderCache.SetAsync(isAsync);
				}
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
					int16_t minHalfGrid = 0;
					int16_t maxHalfGrid = 10;
					ImGui::SliderScalar("Half Grid", ImGuiDataType_S16,
						&OverheadBuilder::Instance().halfGrid, &minHalfGrid, &maxHalfGrid);
					ImGui::SliderFloat("Shooting Time", &OverheadBuilder::Instance().shootingTime,
						0.f, 24.f);
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

		if (player != nullptr && player->Get3D() != nullptr)
		{
			if (PushingCollapsingHeader("Nif Editor"))
			{
				SMainWindow::NifEditor("NifEditor", *static_cast<RE::NiNode*>(player->Get3D()));
				ImGui::TreePop();
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

		if (PushingCollapsingHeader("Combat Behavior Trees Viewer"))
		{
			const auto& trees = RE::CombatBehaviorTree::GetStorage();

			for (const auto& [name, tree] : trees)
			{
				if (tree != nullptr)
				{
					if (PushingCollapsingHeader(tree->name.c_str()))
					{
						if (tree->rootNode != nullptr)
						{
							SMainWindow::CombatBehaviorTreeNodeViewer(*tree->rootNode);
						}

						ImGui::TreePop();
					}
				}
			}

			ImGui::TreePop();
		}

		ImGui::End();
	}
}
