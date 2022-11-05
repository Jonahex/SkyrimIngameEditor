#include "Gui/ImageSpaceEditor.h"

#include "Gui/Utils.h"

#include <RE/T/TESImageSpace.h>

namespace SIE
{
	namespace SImageSpaceEditor
	{
		const std::map<RE::ImageSpaceBaseData::DepthOfField::SkyBlurRadius, const char*>
			BlurRadiusNames{ { { RE::ImageSpaceBaseData::DepthOfField::SkyBlurRadius::kRadius0,
								   "0" },
				{ RE::ImageSpaceBaseData::DepthOfField::SkyBlurRadius::kRadius1, "1" },
				{ RE::ImageSpaceBaseData::DepthOfField::SkyBlurRadius::kRadius2, "2" },
				{ RE::ImageSpaceBaseData::DepthOfField::SkyBlurRadius::kRadius3, "3" },
				{ RE::ImageSpaceBaseData::DepthOfField::SkyBlurRadius::kRadius4, "4" },
				{ RE::ImageSpaceBaseData::DepthOfField::SkyBlurRadius::kRadius5, "5" },
				{ RE::ImageSpaceBaseData::DepthOfField::SkyBlurRadius::kRadius6, "6" },
				{ RE::ImageSpaceBaseData::DepthOfField::SkyBlurRadius::kRadius7, "7" },
				{ RE::ImageSpaceBaseData::DepthOfField::SkyBlurRadius::kNoSky_Radius0, "NoSky 0" },
				{ RE::ImageSpaceBaseData::DepthOfField::SkyBlurRadius::kNoSky_Radius1, "NoSky 1" },
				{ RE::ImageSpaceBaseData::DepthOfField::SkyBlurRadius::kNoSky_Radius2, "NoSky 2" },
				{ RE::ImageSpaceBaseData::DepthOfField::SkyBlurRadius::kNoSky_Radius3, "NoSky 3" },
				{ RE::ImageSpaceBaseData::DepthOfField::SkyBlurRadius::kNoSky_Radius4, "NoSky 4" },
				{ RE::ImageSpaceBaseData::DepthOfField::SkyBlurRadius::kNoSky_Radius5, "NoSky 5" },
				{ RE::ImageSpaceBaseData::DepthOfField::SkyBlurRadius::kNoSky_Radius6, "NoSky 6" },
				{ RE::ImageSpaceBaseData::DepthOfField::SkyBlurRadius::kNoSky_Radius7,
					"NoSky 7" } } };
	}

	void ImageSpaceEditor(const char* label, RE::TESImageSpace& imageSpace)
	{
		ImGui::PushID(label);

		if (PushingCollapsingHeader("HDR"))
		{
			FormEditor(&imageSpace,
				ImGui::DragFloat("Eye Adapt Speed", &imageSpace.data.hdr.eyeAdaptSpeed));
			FormEditor(&imageSpace,
				ImGui::DragFloat("Eye Adapt Strength", &imageSpace.data.hdr.eyeAdaptStrength));
			FormEditor(&imageSpace,
				ImGui::DragFloat("Bloom Blur Radius", &imageSpace.data.hdr.bloomBlurRadius));
			FormEditor(&imageSpace,
				ImGui::DragFloat("Bloom Threshold", &imageSpace.data.hdr.bloomThreshold, 0.1f));
			FormEditor(&imageSpace,
				ImGui::DragFloat("Bloom Scale", &imageSpace.data.hdr.bloomScale, 0.1f));
			FormEditor(&imageSpace, ImGui::DragFloat("Receive Bloom Threshold",
										&imageSpace.data.hdr.receiveBloomThreshold, 0.1f));
			FormEditor(&imageSpace, ImGui::DragFloat("White", &imageSpace.data.hdr.white, 0.1f));
			FormEditor(&imageSpace,
				ImGui::DragFloat("Sunlight Scale", &imageSpace.data.hdr.sunlightScale, 0.1f));
			FormEditor(&imageSpace,
				ImGui::DragFloat("Sky Scale", &imageSpace.data.hdr.skyScale, 0.1f));
			ImGui::TreePop();
		}
		if (PushingCollapsingHeader("Cinematic"))
		{
			FormEditor(&imageSpace,
				ImGui::DragFloat("Saturation", &imageSpace.data.cinematic.saturation, 0.1f));
			FormEditor(&imageSpace,
				ImGui::DragFloat("Tint Amount", &imageSpace.data.tint.amount, 0.1f));
			FormEditor(&imageSpace,
				ImGui::ColorEdit3("Tint Color", &imageSpace.data.tint.color.red));
			FormEditor(&imageSpace,
				ImGui::DragFloat("Brightness", &imageSpace.data.cinematic.brightness, 0.1f));
			FormEditor(&imageSpace,
				ImGui::DragFloat("Contrast", &imageSpace.data.cinematic.contrast, 0.1f));
			ImGui::TreePop();
		}
		if (PushingCollapsingHeader("Depth of Field"))
		{
			FormEditor(&imageSpace,
				ImGui::DragFloat("Strength", &imageSpace.data.depthOfField.strength, 0.1f));
			FormEditor(&imageSpace,
				ImGui::DragFloat("Distance", &imageSpace.data.depthOfField.distance, 50.f));
			FormEditor(&imageSpace,
				ImGui::DragFloat("Range", &imageSpace.data.depthOfField.range, 50.f));
			if (ImGui::BeginCombo("Blur Radius",
					SImageSpaceEditor::BlurRadiusNames.at(
						imageSpace.data.depthOfField.skyBlurRadius.get())))
			{
				for (const auto& [radius, name] : SImageSpaceEditor::BlurRadiusNames)
				{
					const bool isSelected =
						radius == imageSpace.data.depthOfField.skyBlurRadius.get();
					if (ImGui::Selectable(SImageSpaceEditor::BlurRadiusNames.at(radius),
							isSelected))
					{
						imageSpace.data.depthOfField.skyBlurRadius = radius;
					}
				}
				ImGui::EndCombo();
			}
			ImGui::TreePop();
		}

		ImGui::PopID();
	}
}
