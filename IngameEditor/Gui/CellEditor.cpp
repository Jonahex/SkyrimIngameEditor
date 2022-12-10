#include "Gui/CellEditor.h"

#include "Gui/DirectionalAmbientLightingColorsEditor.h"
#include "Gui/ImageSpaceEditor.h"
#include "Gui/LightingTemplateEditor.h"
#include "Gui/Utils.h"
#include "Gui/WaterEditor.h"
#include "Utils/Engine.h"

#include <RE/B/BGSLightingTemplate.h>
#include <RE/B/BSTriShape.h>
#include <RE/B/BSWaterShaderMaterial.h>
#include <RE/B/BSWaterShaderProperty.h>
#include <RE/E/ExtraCellImageSpace.h>
#include <RE/E/ExtraCellSkyRegion.h>
#include <RE/E/ExtraCellWaterEnvMap.h>
#include <RE/E/ExtraCellWaterType.h>
#include <RE/N/NiAVObject.h>
#include <RE/N/NiRTTI.h>
#include <RE/N/NiSourceTexture.h>
#include <RE/S/Sky.h>
#include <RE/T/TESImageSpace.h>
#include <RE/T/TESObjectACTI.h>
#include <RE/T/TESObjectCELL.h>
#include <RE/T/TESRegion.h>
#include <RE/T/TESWaterDisplacement.h>
#include <RE/T/TESWaterForm.h>
#include <RE/T/TESWaterNormals.h>
#include <RE/T/TESWaterObject.h>
#include <RE/T/TESWaterReflections.h>
#include <RE/T/TESWaterSystem.h>

#include <imgui.h>

namespace SIE
{
	namespace SCellEditor
	{
		void UpdateCellWater(RE::TESObjectCELL& cell)
		{
			auto waterSystem = RE::TESWaterSystem::GetSingleton();

			for (const auto& item : cell.references)
			{
				if (item->IsWater())
				{
					//auto geometry = item->Get3D2();
					//UpdateWaterGeometry(geometry, item->GetBaseObject()->GetWaterType());
				}
			}
		}
	}

    void CellEditor(const char* label, RE::TESObjectCELL& cell)
	{
		ImGui::PushID(label);

		RE::TESImageSpace* imageSpace = nullptr;
		auto extraImageSpace = static_cast<RE::ExtraCellImageSpace*>(
			cell.extraList.GetByType(RE::ExtraDataType::kCellImageSpace));
		if (extraImageSpace != nullptr)
		{
			imageSpace = extraImageSpace->imageSpace;
		}
		if (PushingCollapsingHeader("Imagespace"))
		{
			if (FormEditor(&cell, FormSelector<true>("Imagespace", imageSpace)))
			{
				if (imageSpace != nullptr)
				{
					if (extraImageSpace != nullptr)
					{
						extraImageSpace->imageSpace = imageSpace;
					}
					else
					{
						extraImageSpace = new RE::ExtraCellImageSpace;
						extraImageSpace->imageSpace = imageSpace;
						cell.extraList.Add(extraImageSpace);
					}

					static REL::Relocation<void (*)(void*, const RE::ImageSpaceBaseData&)>
						imageSpaceUpdate{ REL::ID(105683) };
					static REL::Relocation<void**> imageSpaceEffectManager{ REL::ID(414660) };

					imageSpaceUpdate(*imageSpaceEffectManager, imageSpace->data);
				}
				else if (extraImageSpace != nullptr)
				{
					cell.extraList.Remove(extraImageSpace);
				}
			}
			if (imageSpace != nullptr)
			{
				ImageSpaceEditor("ImageSpaceEditor", *imageSpace);
			}
			ImGui::TreePop();
		}

		if (cell.IsInteriorCell())
		{
			const auto data = cell.cellData.interior;

			if (PushingCollapsingHeader("Lighting"))
			{
				using enum RE::INTERIOR_DATA::Inherit;

				if (PushingCollapsingHeader("Lighting Template"))
				{
					FormEditor(&cell,
						FormSelector<true>("Lighting Template", cell.lightingTemplate));
					if (cell.lightingTemplate != nullptr)
					{
						LightingTemplateEditor("##LightingTemplateEditor", *cell.lightingTemplate);
					}
					ImGui::TreePop();
				}
				if (PushingCollapsingHeader("Inherit"))
				{
					if (ImGui::BeginTable("InheritFlags", 4))
					{
						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						FormEditor(&cell,
							FlagEdit("Ambient Color", data->lightingTemplateInheritanceFlags,
								kAmbientColor));
						ImGui::TableNextColumn();
						FormEditor(&cell,
							FlagEdit("Directional Color", data->lightingTemplateInheritanceFlags,
								kDirectionalColor));
						ImGui::TableNextColumn();
						FormEditor(&cell,
							FlagEdit("Direcitonal Rotation", data->lightingTemplateInheritanceFlags,
								kDirectionalRotation));
						ImGui::TableNextColumn();
						FormEditor(&cell,
							FlagEdit("Directional Fade", data->lightingTemplateInheritanceFlags,
								kDirectionalFade));
						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						FormEditor(&cell, FlagEdit("Fog Color",
											  data->lightingTemplateInheritanceFlags, kFogColor));
						ImGui::TableNextColumn();
						FormEditor(&cell,
							FlagEdit("Fog Near", data->lightingTemplateInheritanceFlags, kFogNear));
						ImGui::TableNextColumn();
						FormEditor(&cell,
							FlagEdit("Fog Far", data->lightingTemplateInheritanceFlags, kFogFar));
						ImGui::TableNextColumn();
						FormEditor(&cell, FlagEdit("Fog Power",
											  data->lightingTemplateInheritanceFlags, kFogPower));
						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						FormEditor(&cell,
							FlagEdit("Fog Max", data->lightingTemplateInheritanceFlags, kFogMax));
						ImGui::TableNextColumn();
						FormEditor(&cell,
							FlagEdit("Clip Distance", data->lightingTemplateInheritanceFlags,
								kClipDistance));
						ImGui::TableNextColumn();
						FormEditor(&cell,
							FlagEdit("Light Fade Distances", data->lightingTemplateInheritanceFlags,
								kLightFadeDistances));
						ImGui::EndTable();
					}
					ImGui::TreePop();
				}
				if (!data->lightingTemplateInheritanceFlags.all(kAmbientColor))
				{
					if (PushingCollapsingHeader("Ambient"))
					{
						FormEditor(&cell, ColorEdit("Ambient Color", data->ambient));
						FormEditor(&cell, DirectionalAmbientLightingColorsEditor("##DALC",
											  data->directionalAmbientLightingColors));
						ImGui::TreePop();
					}
				}
				if (!data->lightingTemplateInheritanceFlags.all(kFogColor, kFogNear, kFogFar,
						kFogPower, kFogMax, kClipDistance))
				{
					if (PushingCollapsingHeader("Fog"))
					{
						if (data->lightingTemplateInheritanceFlags.none(kFogColor))
						{
							FormEditor(&cell, ColorEdit("Near Color", data->fogColorNear));
							FormEditor(&cell, ColorEdit("Far Color", data->fogColorFar));
						}
						if (data->lightingTemplateInheritanceFlags.none(kFogNear))
						{
							FormEditor(&cell, ImGui::DragFloat("Near", &data->fogNear, 50.f));
						}
						if (data->lightingTemplateInheritanceFlags.none(kFogFar))
						{
							FormEditor(&cell, ImGui::DragFloat("Far", &data->fogFar, 50.f));
						}
						if (data->lightingTemplateInheritanceFlags.none(kFogPower))
						{
							FormEditor(&cell, ImGui::DragFloat("Power", &data->fogPower, 0.1f));
						}
						if (data->lightingTemplateInheritanceFlags.none(kFogMax))
						{
							FormEditor(&cell, ImGui::DragFloat("Max", &data->fogClamp, 0.1f));
						}
						if (data->lightingTemplateInheritanceFlags.none(kClipDistance))
						{
							FormEditor(&cell,
								ImGui::DragFloat("Clip Distance", &data->clipDist, 50.f));
						}
						ImGui::TreePop();
					}
				}
				if (!data->lightingTemplateInheritanceFlags.all(kDirectionalColor,
						kDirectionalRotation, kDirectionalFade))
				{
					if (PushingCollapsingHeader("Directional"))
					{
						if (data->lightingTemplateInheritanceFlags.none(kDirectionalColor))
						{
							FormEditor(&cell, ColorEdit("Color", data->directional));
						}
						if (data->lightingTemplateInheritanceFlags.none(kDirectionalRotation))
						{
							FormEditor(&cell, ImGui::DragScalarN("Rotation", ImGuiDataType_U32,
												  &data->directionalXY, 2, 10.f));
						}
						if (data->lightingTemplateInheritanceFlags.none(kDirectionalFade))
						{
							FormEditor(&cell,
								ImGui::DragFloat("Fade", &data->directionalFade, 0.1f));
						}
						ImGui::TreePop();
					}
				}
				if (data->lightingTemplateInheritanceFlags.none(kLightFadeDistances))
				{
					FormEditor(&cell, ImGui::DragFloat2("Light Fade Distances", &data->lightFadeStart, 50.f));
				}

				const auto sky = RE::Sky::GetSingleton();

				if (FormEditor(&cell,
					FlagEdit("Show Sky", cell.cellFlags, RE::TESObjectCELL::Flag::kShowSky)))
				{
					if (cell.cellFlags.any(RE::TESObjectCELL::Flag::kShowSky))
					{
						cell.extraList.Add(new RE::ExtraCellSkyRegion);
					}
					else
					{
						cell.extraList.Remove(cell.extraList.GetByType(RE::ExtraDataType::kCellSkyRegion));
					}
					sky->ResetWeather();
				}
				if (cell.cellFlags.any(RE::TESObjectCELL::Flag::kShowSky))
				{
					auto extraData = static_cast<RE::ExtraCellSkyRegion*>(
						cell.extraList.GetByType(RE::ExtraDataType::kCellSkyRegion));
					if (FormEditor(&cell, FormSelector<true>("Region", extraData->skyRegion)))
					{
						sky->ResetWeather();
					}
					if (FormEditor(&cell,
						FlagEdit("Use Sky Lighting", cell.cellFlags, RE::TESObjectCELL::Flag::kUseSkyLighting)))
					{
						sky->ResetWeather();
					}
				}
				ImGui::TreePop();
			}
		}

		if (cell.cellFlags.any(RE::TESObjectCELL::Flag::kHasWater))
		{
			if (const auto extraWater = static_cast<RE::ExtraCellWaterType*>(cell.extraList.GetByType(RE::ExtraDataType::kCellWaterType)))
			{
				if (extraWater->water != nullptr)
				{
					if (PushingCollapsingHeader("Water"))
					{
						FormEditor(&cell, FormSelector<false>("Water Type", extraWater->water));
						if (WaterEditor("WaterEditor", *extraWater->water))
						{
							SCellEditor::UpdateCellWater(cell);
						}
						ImGui::TreePop();
					}
				}
			}

			if (cell.IsInteriorCell())
			{
				RE::BSFixedString envMapPath;
				auto extraWaterEnvMap = static_cast<RE::ExtraCellWaterEnvMap*>(
					cell.extraList.GetByType(RE::ExtraDataType::kCellWaterEnvMap));
				if (extraWaterEnvMap != nullptr)
				{
					envMapPath = extraWaterEnvMap->waterEnvMap.textureName;
				}
				if (FormEditor(&cell, TexturePathEdit("Water Environment Map",
										  "Water Environment Map", envMapPath)))
				{
					if (envMapPath.empty())
					{
						cell.extraList.Remove(extraWaterEnvMap);
					}
					else
					{
						if (extraWaterEnvMap == nullptr)
						{
							extraWaterEnvMap = new RE::ExtraCellWaterEnvMap;
							cell.extraList.Add(extraWaterEnvMap);
						}
						extraWaterEnvMap->waterEnvMap.textureName = envMapPath;
					}
				}
			}
		}

		ImGui::PopID();
    }
}
