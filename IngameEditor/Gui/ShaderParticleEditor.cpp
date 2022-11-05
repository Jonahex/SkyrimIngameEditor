#include "Gui/ShaderParticleEditor.h"

#include "Gui/Utils.h"

#include <RE/B/BGSShaderParticleGeometryData.h>

namespace SIE
{
	bool ShaderParticleEditor(const char* label, RE::BGSShaderParticleGeometryData& data)
    {
		using enum RE::BGSShaderParticleGeometryData::DataID;

		ImGui::PushID(label);

		constexpr uint32_t minInt = 0;
		constexpr uint32_t minNumSubtextures = 1;
		constexpr uint32_t maxNumSubtextures = 100;
		constexpr uint32_t maxBoxSize = 10000;

		bool isEdited = false;

		if (FormEditor(&data, EnumSelector<RE::BGSShaderParticleGeometryData::ParticleType>(
								  "Shader", data.data[static_cast<size_t>(kParticleType)].i)))
		{
			isEdited = true;
		}
        if (FormEditor(&data,
						 ImGui::SliderScalarN("Box Size", ImGuiDataType_U32,
					&data.data[static_cast<size_t>(kBoxSize)].i, 1, &minInt, &maxBoxSize)))
		{
			isEdited = true;
		}
        if (FormEditor(&data, ImGui::SliderFloat2("Particle Size",
								  &data.data[static_cast<size_t>(kParticleSizeX)].f, 0.f, 100.f)))
		{
			isEdited = true;
		}
        if (FormEditor(&data,
						 ImGui::SliderFloat2("Center Offset",
					&data.data[static_cast<size_t>(kCenterOffsetMin)].f, -100.f, 100.f)))
		{
			isEdited = true;
		}
        if (FormEditor(&data, ImGui::SliderScalarN("# of subtextures", ImGuiDataType_U32,
								  &data.data[static_cast<size_t>(kNumSubtexturesX)].i, 2,
								  &minNumSubtextures,
								  &maxNumSubtextures)))
		{
			isEdited = true;
		}
        if (FormEditor(&data,
						 ImGui::SliderFloat("Particle Density",
							  &data.data[static_cast<size_t>(kParticleDensity)].f, 0.f, 1000.f)))
		{
			isEdited = true;
		}
        if (FormEditor(&data,
						 ImGui::SliderFloat("Gravity Velocity",
							  &data.data[static_cast<size_t>(kGravityVelocity)].f, 0.f, 1000.f)))
		{
			isEdited = true;
		}
        if (FormEditor(&data,
						 ImGui::SliderFloat("Rotation Velocity",
							  &data.data[static_cast<size_t>(kRotationVelocity)].f, 0.f, 1000.f)))
		{
			isEdited = true;
		}
        if (FormEditor(&data,
						 SliderAngleDegree("Initial Rotation Range",
							  &data.data[static_cast<size_t>(kStartRotationRange)].f, 0.f, 360.f)))
		{
			isEdited = true;
		}
		if (FormEditor(&data, TexturePathEdit("ShaderParticlePath", "Path", data.particleTexture.textureName)))
		{
			isEdited = true;
		}

		ImGui::PopID();

        return isEdited;
    }
}
