#include "Gui/LightingTemplateEditor.h"

#include "Gui/DirectionalAmbientLightingColorsEditor.h"
#include "Gui/Utils.h"

#include <RE/B/BGSLightingTemplate.h>

#include <imgui.h>

namespace SIE
{
	void LightingTemplateEditor(const char* label, RE::BGSLightingTemplate& lightingTemplate) 
	{
		ImGui::PushID(label);

		if (PushingCollapsingHeader("Ambient"))
		{
			FormEditor(&lightingTemplate,
				ColorEdit("Ambient Color", lightingTemplate.data.ambient));
			FormEditor(&lightingTemplate, DirectionalAmbientLightingColorsEditor("##DALC",
					lightingTemplate.directionalAmbientLightingColors));
			ImGui::TreePop();
		}
		if (PushingCollapsingHeader("Fog"))
		{
			FormEditor(&lightingTemplate,
				ColorEdit("Near Color", lightingTemplate.data.fogColorNear));
			FormEditor(&lightingTemplate,
				ColorEdit("Far Color", lightingTemplate.data.fogColorFar));
			FormEditor(&lightingTemplate,
				ImGui::DragFloat("Near", &lightingTemplate.data.fogNear, 50.f));
			FormEditor(&lightingTemplate,
				ImGui::DragFloat("Far", &lightingTemplate.data.fogFar, 50.f));
			FormEditor(&lightingTemplate,
				ImGui::DragFloat("Power", &lightingTemplate.data.fogPower, 0.1f));
			FormEditor(&lightingTemplate,
				ImGui::DragFloat("Max", &lightingTemplate.data.fogClamp, 0.1f));
			FormEditor(&lightingTemplate,
				ImGui::DragFloat("Clip Distance", &lightingTemplate.data.clipDist, 50.f));
			ImGui::TreePop();
		}
		if (PushingCollapsingHeader("Directional"))
		{
			FormEditor(&lightingTemplate, ColorEdit("Color", lightingTemplate.data.directional));
			FormEditor(&lightingTemplate, ImGui::DragScalarN("Rotation", ImGuiDataType_U32,
											  &lightingTemplate.data.directionalXY, 2, 10.f));
			FormEditor(&lightingTemplate,
				ImGui::DragFloat("Fade", &lightingTemplate.data.directionalFade, 0.1f));
			ImGui::TreePop();
		}
		FormEditor(&lightingTemplate,
			ImGui::DragFloat2("Light Fade Distances", &lightingTemplate.data.lightFadeStart, 50.f));

		ImGui::PopID();
	}
}
