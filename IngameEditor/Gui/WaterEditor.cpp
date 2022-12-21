#include "Gui/WaterEditor.h"

#include "Gui/ImageSpaceEditor.h"
#include "Gui/Utils.h"

#include <RE/B/BGSMaterialType.h>
#include <RE/B/BGSSoundDescriptorForm.h>
#include <RE/S/SpellItem.h>
#include <RE/T/TESImageSpace.h>
#include <RE/T/TESWaterForm.h>
#include <RE/T/TESWaterSystem.h>

#include <imgui.h>

namespace SIE
{
	namespace SWaterEditor
	{
		bool NoiseEditor(RE::TESWaterForm& water, size_t layerIndex)
		{
			bool wasEdited = false;

			if (FormEditor(&water, SliderAngleDegree("Wind Direction",
				&water.data.noiseWindDirectionA[layerIndex], 0.f, 360.f)))
			{
				wasEdited = true;
			}
			if (FormEditor(&water, ImGui::SliderFloat("Wind Speed",
								   &water.data.noiseWindSpeedA[layerIndex], 0.f, 1.f)))
			{
				wasEdited = true;
			}
			if (FormEditor(&water, ImGui::SliderFloat("Amplitude Scale",
									   &water.data.amplitudeA[layerIndex], 0.f, 1.f)))
			{
				wasEdited = true;
			}
			if (FormEditor(&water,
				ImGui::SliderFloat("UV Scale", &water.data.uvScaleA[layerIndex], 0.f, 10000.f)))
			{
				wasEdited = true;
			}
			if (FormEditor(&water, TexturePathEdit("##NoiseTexturePath", "Texture Path",
								   water.noiseTextures[layerIndex].textureName)))
			{
				wasEdited = true;
			}

			return wasEdited;
		}
	}

    bool WaterEditor(const char* label, RE::TESWaterForm& water)
	{
		bool needsReset = false;

		ImGui::PushID(label);

		FormEditor(&water,
			FlagEdit("Causes Damage", water.flags, RE::TESWaterForm::Flag::kCauseDamage));
		if (water.flags.any(RE::TESWaterForm::Flag::kCauseDamage))
		{
			constexpr uint16_t minDamage = 0;
			constexpr uint16_t maxDamage = 1000;
			FormEditor(&water, ImGui::DragScalarN("Damage", ImGuiDataType_U16, &water.attackDamage,
								   1, 1, &minDamage, &maxDamage));
		}

		FormEditor(&water, FormSelector<true>("Spell", water.contactSpell));

		if (PushingCollapsingHeader("Imagespace"))
		{
			if (FormEditor(&water, FormSelector<false>("Imagespace", water.imageSpace)))
			{
				const auto waterSystem = RE::TESWaterSystem::GetSingleton();
				if (waterSystem->playerUnderwater)
				{
					static REL::Relocation<void (RE::TESWaterSystem*, bool, float)>
						setPlayerUnderwater{ RELOCATION_ID(31409, 32216) };

					setPlayerUnderwater(waterSystem, !waterSystem->playerUnderwater,
						waterSystem->underwaterHeight);
					setPlayerUnderwater(waterSystem, !waterSystem->playerUnderwater,
						waterSystem->underwaterHeight);
				}
				needsReset = true;
			}
			ImageSpaceEditor("ImageSpaceEditor", *water.imageSpace);
			ImGui::TreePop();
		}

		if (PushingCollapsingHeader("Color Properties"))
		{
			if (FormEditor(&water, ColorEdit("Shallow Color", water.data.shallowWaterColor)))
			{
				needsReset = true;
			}
			if (FormEditor(&water, ColorEdit("Deep Color", water.data.deepWaterColor)))
			{
				needsReset = true;
			}
			if (FormEditor(&water, ColorEdit("Reflection Color", water.data.reflectionWaterColor)))
			{
				needsReset = true;
			}
			ImGui::TreePop();
		}

		if (PushingCollapsingHeader("Water Properties"))
		{
			FormEditor(&water, FormSelector<true>("Sound", water.waterSound));
			FormEditor(&water, FormSelector<true>("Material Type", water.materialType));

			if (PushingCollapsingHeader("Optical Properties"))
			{
				{
					constexpr int8_t minOpacity = 0;
					constexpr int8_t maxOpacity = 100;
					if (FormEditor(&water, ImGui::SliderScalar("Opacity", ImGuiDataType_S8,
											   &water.alpha, &minOpacity, &maxOpacity)))
					{
						needsReset = true;
					}
				}
				if (FormEditor(&water, ImGui::SliderFloat("Reflectivity Amount",
										   &water.data.reflectionAmount, 0.f, 1.f)))
				{
					needsReset = true;
				}
				if (FormEditor(&water, ImGui::SliderFloat("Reflection Magnitude",
										   &water.data.reflectionMagnitude, 0.f, 1.f)))
				{
					needsReset = true;
				}
				if (FormEditor(&water, ImGui::SliderFloat("Refraction Magnitude",
										   &water.data.refractionMagnitude, 0.f, 100.f)))
				{
					needsReset = true;
				}
				if (FormEditor(&water,
						ImGui::SliderFloat("Fresnel Amount", &water.data.fresnelAmount, 0.f, 1.f)))
				{
					needsReset = true;
				}
				ImGui::TreePop();
			}
			if (PushingCollapsingHeader("Displacement Simulator"))
			{
				if (FormEditor(&water,
					ImGui::SliderFloat("Force", &water.data.displacementForce, 0.f, 1.f)))
				{
					needsReset = true;
				}
				if (FormEditor(&water,
					ImGui::SliderFloat("Velocity", &water.data.displacementVelocity, 0.f, 1.f)))
				{
					needsReset = true;
				}
				if (FormEditor(&water,
						ImGui::SliderFloat("Falloff", &water.data.displacementFalloff, 0.f, 1.f)))
				{
					needsReset = true;
				}
				if (FormEditor(&water, ImGui::SliderFloat("Dampener",
										   &water.data.displacementDampener, 0.f, 20.f)))
				{
					needsReset = true;
				}
				if (FormEditor(&water, ImGui::SliderFloat("Starting Size",
										   &water.data.displacementSize, 0.f, 1.f)))
				{
					needsReset = true;
				}
				ImGui::TreePop();
			}
			if (FormEditor(&water,
				ImGui::DragFloat3("Linear Velocity", &water.linearVelocity.x, 0.1f, -100.f, 100.f)))
			{
				needsReset = true;
			}
			if (FormEditor(&water, ImGui::DragFloat3("Angular Velocity", &water.angularVelocity.x,
									   0.1f, -100.f, 100.f)))
			{
				needsReset = true;
			}
			ImGui::TreePop();
		}

		if (PushingCollapsingHeader("Specular Properties"))
		{
			if (FormEditor(&water, ImGui::SliderFloat("Sun Specular Power",
									   &water.data.sunSpecularPower, 0.f, 10000.f)))
			{
				needsReset = true;
			}
			if (FormEditor(&water, ImGui::SliderFloat("Sun Specular Magnitude",
									   &water.data.sunSpecularMagnitude, 0.f, 10.f)))
			{
				needsReset = true;
			}
			if (FormEditor(&water, ImGui::SliderFloat("Sun Sparkle Power",
									   &water.data.sunSparklePower, 0.f, 10000.f)))
			{
				needsReset = true;
			}
			if (FormEditor(&water, ImGui::SliderFloat("Sun Sparkle Magnitude",
									   &water.data.sunSparkleMagnitude, 0.f, 10.f)))
			{
				needsReset = true;
			}
			if (FormEditor(&water, ImGui::SliderFloat("Specular Radius", &water.data.specularRadius,
									   0.f, 10000.f)))
			{
				needsReset = true;
			}
			if (FormEditor(&water, ImGui::SliderFloat("Specular Brightness",
									   &water.data.specularBrightness, 0.f, 100.f)))
			{
				needsReset = true;
			}
			if (FormEditor(&water,
					ImGui::SliderFloat("Specular Power", &water.data.specularPower, 0.f, 10000.f)))
			{
				needsReset = true;
			}
			ImGui::TreePop();
		}

		if (PushingCollapsingHeader("Noise Properties"))
		{
			if (FormEditor(&water,
				FlagEdit("Enable Flowmap", water.flags, RE::TESWaterForm::Flag::kEnableFlowmap)))
			{
				needsReset = true;
			}
			if (water.flags.any(RE::TESWaterForm::Flag::kEnableFlowmap))
			{
				if (FormEditor(&water, FlagEdit("Blend Normals", water.flags, RE::TESWaterForm::Flag::kBlendNormals)))
				{
					needsReset = true;
				}
				if (FormEditor(&water,
						ImGui::SliderFloat("Flowmap Scale", &water.data.flowmapScale, 0.f, 1.f)))
				{
					needsReset = true;
				}
			}
			if (FormEditor(&water,
					ImGui::SliderFloat("Noise Falloff", &water.data.noiseFalloff, 0.f, 8192.f)))
			{
				needsReset = true;
			}
			if (FormEditor(&water, TexturePathEdit("##FlowNormalsNoise", "Flow Normals Texture Path",
				water.noiseTextures[3].textureName)))
			{
				needsReset = true;
			}

			if (PushingCollapsingHeader("Noise Layer One"))
			{
				if (SWaterEditor::NoiseEditor(water, 0))
				{
					needsReset = true;
				}
				ImGui::TreePop();
			}
			if (PushingCollapsingHeader("Noise Layer Two"))
			{
				if (SWaterEditor::NoiseEditor(water, 1))
				{
					needsReset = true;
				}
				ImGui::TreePop();
			}
			if (PushingCollapsingHeader("Noise Layer Three"))
			{
				if (SWaterEditor::NoiseEditor(water, 2))
				{
					needsReset = true;
				}
				ImGui::TreePop();
			}
			ImGui::TreePop();
		}

		if (PushingCollapsingHeader("Fog Properties"))
		{
			if (PushingCollapsingHeader("Above Water"))
			{
				if (FormEditor(&water, ImGui::SliderFloat("Fog Amount",
										   &water.data.aboveWaterFogAmount, 0.f, 1.f)))
				{
					needsReset = true;
				}
				if (FormEditor(&water, ImGui::DragFloat("Fog Distance - Near Plane",
							&water.data.aboveWaterFogDistNear, 10.f, -10000.f, 10000.f)))
				{
					needsReset = true;
				}
				if (FormEditor(&water, ImGui::DragFloat("Fog Distance - Far Plane",
							&water.data.aboveWaterFogDistFar, 10.f, -10000.f, 10000.f)))
				{
					needsReset = true;
				}
				ImGui::TreePop();
			}
			if (PushingCollapsingHeader("Under Water"))
			{
				if (FormEditor(&water, ImGui::SliderFloat("Fog Amount",
										   &water.data.underwaterFogAmount, 0.f, 1.f)))
				{
					needsReset = true;
				}
				if (FormEditor(&water, ImGui::DragFloat("Fog Distance - Near Plane",
							&water.data.underwaterFogDistNear, 10.f, -10000.f, 10000.f)))
				{
					needsReset = true;
				}
				if (FormEditor(&water, ImGui::DragFloat("Fog Distance - Far Plane",
							&water.data.underwaterFogDistFar, 10.f, -10000.f, 10000.f)))
				{
					needsReset = true;
				}
				ImGui::TreePop();
			}
			ImGui::TreePop();
		}

		if (PushingCollapsingHeader("Depth Properties"))
		{
			if (FormEditor(&water, ImGui::SliderFloat("Reflections",
									   &water.data.depthProperties.reflections, 0.f, 1.f)))
			{
				needsReset = true;
			}
			if (FormEditor(&water, ImGui::SliderFloat("Refraction",
									   &water.data.depthProperties.refraction, 0.f, 1.f)))
			{
				needsReset = true;
			}
			if (FormEditor(&water,
					ImGui::SliderFloat("Normals", &water.data.depthProperties.normals, 0.f, 1.f)))
			{
				needsReset = true;
			}
			if (FormEditor(&water, ImGui::SliderFloat("Specular Lighting",
									   &water.data.depthProperties.specularLighting, 0.f, 1.f)))
			{
				needsReset = true;
			}
			ImGui::TreePop();
		}

		ImGui::PopID();

		return needsReset;
    }
}
