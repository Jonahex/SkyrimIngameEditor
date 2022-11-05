#include "Gui/VolumetricLightingEditor.h"

#include "Gui/Utils.h"

#include <RE/B/BGSVolumetricLighting.h>

namespace SIE
{
	void VolumetricLightingEditor(const char* label, RE::BGSVolumetricLighting& lighting)
	{
		ImGui::PushID(label);

		if (PushingCollapsingHeader("General"))
		{
			FormEditor(&lighting,
				ImGui::DragFloat("Intensity", &lighting.intensity, 0.01f, 0.f, 30.f));
			ImGui::TreePop();
		}
		if (PushingCollapsingHeader("Custom Color"))
		{
			FormEditor(&lighting,
				ImGui::SliderFloat("Contribution",
									  &lighting.customColor.contribution, 0.f, 1.f));
			FormEditor(&lighting, ImGui::ColorEdit3("Color", &lighting.red));
			ImGui::TreePop();
		}
		if (PushingCollapsingHeader("Density"))
		{
			FormEditor(&lighting,
				ImGui::SliderFloat("Contribution", &lighting.density.contribution, 0.f, 1.f));
			FormEditor(&lighting, ImGui::SliderFloat("Size", &lighting.density.size, 0.1f, 10000.f,
									  "%.3f", ImGuiSliderFlags_Logarithmic));
			FormEditor(&lighting,
				ImGui::DragFloat("Wind Speed", &lighting.density.windSpeed, 0.1f, 0.f, 100.f));
			FormEditor(&lighting, ImGui::DragFloat("Falling Speed", &lighting.density.fallingSpeed,
									  0.1f, 0.f, 100.f));
			ImGui::TreePop();
		}
		if (PushingCollapsingHeader("Phase Function"))
		{
			FormEditor(&lighting,
				ImGui::SliderFloat("Contribution", &lighting.phaseFunction.contribution, 0.f, 1.f));
			FormEditor(&lighting,
				ImGui::SliderFloat("Scattering", &lighting.phaseFunction.scattering, -1.f, 1.f));
			ImGui::TreePop();
		}
		if (PushingCollapsingHeader("Sampling Repartition"))
		{
			FormEditor(&lighting, ImGui::DragFloat("Range Factor",
									  &lighting.samplingRepartition.rangeFactor, 0.1f, 1.f, 80.f));
			ImGui::TreePop();
		}

		ImGui::PopID();
	}
}
