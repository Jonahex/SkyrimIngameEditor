#include "Gui/WaterEditor.h"

#include "Gui/ImageSpaceEditor.h"
#include "Gui/Utils.h"

#include <RE/B/BGSMaterialType.h>
#include <RE/B/BGSSoundDescriptorForm.h>
#include <RE/S/SpellItem.h>
#include <RE/T/TESImageSpace.h>
#include <RE/T/TESWaterForm.h>

#include <imgui.h>

namespace SIE
{
	namespace SWaterEditor
	{
		void NoiseEditor(RE::TESWaterForm& water, size_t layerIndex)
		{
			FormEditor(&water, SliderAngleDegree("Wind Direction",
								   &water.data.noiseWindDirectionA[layerIndex], 0.f, 360.f));
			FormEditor(&water, ImGui::SliderFloat("Wind Speed",
								   &water.data.noiseWindSpeedA[layerIndex], 0.f, 1.f));
			FormEditor(&water, ImGui::SliderFloat("Amplitude Scale", &water.data.amplitudeA[layerIndex], 0.f, 1.f));
			FormEditor(&water,
				ImGui::SliderFloat("UV Scale", &water.data.uvScaleA[layerIndex], 0.f, 10000.f));
			FormEditor(&water, TexturePathEdit("##NoiseTexturePath", "Texture Path",
								   water.noiseTextures[layerIndex].textureName));
		}
	}

    void WaterEditor(const char* label, RE::TESWaterForm& water)
    {
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
			FormEditor(&water, FormSelector<false>("Imagespace", water.imageSpace));
			ImageSpaceEditor("ImageSpaceEditor", *water.imageSpace);
			ImGui::TreePop();
		}

		if (PushingCollapsingHeader("Color Properties"))
		{
			FormEditor(&water, ColorEdit("Shallow Color", water.data.shallowWaterColor));
			FormEditor(&water, ColorEdit("Deep Color", water.data.deepWaterColor));
			FormEditor(&water, ColorEdit("Reflection Color", water.data.reflectionWaterColor));
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
					FormEditor(&water, ImGui::SliderScalar("Opacity", ImGuiDataType_S8,
										   &water.alpha, &minOpacity, &maxOpacity));
				}
				FormEditor(&water, ImGui::SliderFloat("Reflectivity Amount",
									   &water.data.reflectionAmount, 0.f, 1.f));
				FormEditor(&water, ImGui::SliderFloat("Reflection Magnitude",
									   &water.data.reflectionMagnitude, 0.f, 1.f));
				FormEditor(&water, ImGui::SliderFloat("Refraction Magnitude",
									   &water.data.refractionMagnitude, 0.f, 100.f));
				FormEditor(&water,
					ImGui::SliderFloat("Fresnel Amount", &water.data.fresnelAmount, 0.f, 1.f));
				ImGui::TreePop();
			}
			if (PushingCollapsingHeader("Displacement Simulator"))
			{
				FormEditor(&water,
					ImGui::SliderFloat("Force", &water.data.displacementForce, 0.f, 1.f));
				FormEditor(&water,
					ImGui::SliderFloat("Velocity", &water.data.displacementVelocity, 0.f, 1.f));
				FormEditor(&water,
					ImGui::SliderFloat("Falloff", &water.data.displacementFalloff, 0.f, 1.f));
				FormEditor(&water,
					ImGui::SliderFloat("Dampener", &water.data.displacementDampener, 0.f, 20.f));
				FormEditor(&water,
					ImGui::SliderFloat("Starting Size", &water.data.displacementSize, 0.f, 1.f));
				ImGui::TreePop();
			}
			FormEditor(&water,
				ImGui::DragFloat3("Linear Velocity", &water.linearVelocity.x, 0.1f, -100.f, 100.f));
			FormEditor(&water,
				ImGui::DragFloat3("Angular Velocity", &water.angularVelocity.x, 0.1f, -100.f, 100.f));
			ImGui::TreePop();
		}

		if (PushingCollapsingHeader("Specular Properties"))
		{
		    FormEditor(&water, ImGui::SliderFloat("Sun Specular Power",
								   &water.data.sunSpecularPower, 0.f, 10000.f));
			FormEditor(&water, ImGui::SliderFloat("Sun Specular Magnitude",
								   &water.data.sunSpecularMagnitude, 0.f, 10.f));
			FormEditor(&water, ImGui::SliderFloat("Sun Sparkle Power",
								   &water.data.sunSparklePower, 0.f, 10000.f));
			FormEditor(&water, ImGui::SliderFloat("Sun Sparkle Magnitude",
								   &water.data.sunSparkleMagnitude, 0.f, 10.f));
			FormEditor(&water, ImGui::SliderFloat("Specular Radius",
								   &water.data.specularRadius, 0.f, 10000.f));
			FormEditor(&water, ImGui::SliderFloat("Specular Brightness",
								   &water.data.specularBrightness, 0.f, 100.f));
			FormEditor(&water, ImGui::SliderFloat("Specular Power", &water.data.specularPower, 0.f, 10000.f));
			FormEditor(&water,
				ImGui::SliderFloat("UnkA0", &water.data.unkA0, 0.f, 100.f));
			ImGui::TreePop();
		}

		if (PushingCollapsingHeader("Noise Properties"))
		{
			FormEditor(&water,
				FlagEdit("Enable Flowmap", water.flags, RE::TESWaterForm::Flag::kEnableFlowmap));
			if (water.flags.any(RE::TESWaterForm::Flag::kEnableFlowmap))
			{
				FormEditor(&water, FlagEdit("Blend Normals", water.flags, RE::TESWaterForm::Flag::kBlendNormals));
				FormEditor(&water,
					ImGui::SliderFloat("Flowmap Scale", &water.data.flowmapScale, 0.f, 1.f));
			}
			FormEditor(&water,
				ImGui::SliderFloat("Noise Falloff", &water.data.noiseFalloff, 0.f, 8192.f));
			FormEditor(&water, TexturePathEdit("##FlowNormalsNoise", "Flow Normals Texture Path",
								   water.noiseTextures[3].textureName));

			if (PushingCollapsingHeader("Noise Layer One"))
			{
				SWaterEditor::NoiseEditor(water, 0);
				ImGui::TreePop();
			}
			if (PushingCollapsingHeader("Noise Layer Two"))
			{
				SWaterEditor::NoiseEditor(water, 1);
				ImGui::TreePop();
			}
			if (PushingCollapsingHeader("Noise Layer Three"))
			{
				SWaterEditor::NoiseEditor(water, 2);
				ImGui::TreePop();
			}
			ImGui::TreePop();
		}

		if (PushingCollapsingHeader("Fog Properties"))
		{
			if (PushingCollapsingHeader("Above Water"))
			{
				FormEditor(&water,
					ImGui::SliderFloat("Fog Amount", &water.data.aboveWaterFogAmount, 0.f, 1.f));
				FormEditor(&water, ImGui::DragFloat("Fog Distance - Near Plane",
									   &water.data.aboveWaterFogDistNear, 10.f, -10000.f, 10000.f));
				FormEditor(&water, ImGui::DragFloat("Fog Distance - Far Plane",
									   &water.data.aboveWaterFogDistFar, 10.f, -10000.f, 10000.f));
				ImGui::TreePop();
			}
			if (PushingCollapsingHeader("Under Water"))
			{
				FormEditor(&water,
					ImGui::SliderFloat("Fog Amount", &water.data.underwaterFogAmount, 0.f, 1.f));
				FormEditor(&water, ImGui::DragFloat("Fog Distance - Near Plane",
									   &water.data.underwaterFogDistNear, 10.f, -10000.f, 10000.f));
				FormEditor(&water, ImGui::DragFloat("Fog Distance - Far Plane",
									   &water.data.underwaterFogDistFar, 10.f, -10000.f, 10000.f));
				ImGui::TreePop();
			}
			ImGui::TreePop();
		}

		if (PushingCollapsingHeader("Depth Properties"))
		{
			FormEditor(&water,
				ImGui::SliderFloat("Reflections", &water.data.depthProperties.reflections, 0.f, 1.f));
			FormEditor(&water,
				ImGui::SliderFloat("Refraction", &water.data.depthProperties.refraction, 0.f, 1.f));
			FormEditor(&water,
				ImGui::SliderFloat("Normals", &water.data.depthProperties.normals, 0.f, 1.f));
			FormEditor(&water,
				ImGui::SliderFloat("Specular Lighting", &water.data.depthProperties.specularLighting, 0.f, 1.f));
			ImGui::TreePop();
		}

		ImGui::PopID();
    }
}
