#include "Gui/DirectionalAmbientLightingColorsEditor.h"

#include "Gui/Utils.h"

namespace SIE
{
	bool DirectionalAmbientLightingColorsDirectionalEditor(const char* label,
		RE::BGSDirectionalAmbientLightingColors::Directional& directional)
	{
		ImGui::PushID(label);

		bool wasEdited = false;
		if (ColorEdit("X Min", directional.x.min))
		{
			wasEdited = true;
		}
		if (ColorEdit("X Max", directional.x.max))
		{
			wasEdited = true;
		}
		if (ColorEdit("Y Min", directional.y.min))
		{
			wasEdited = true;
		}
		if (ColorEdit("Y Max", directional.y.max))
		{
			wasEdited = true;
		}
		if (ColorEdit("Z Min", directional.z.min))
		{
			wasEdited = true;
		}
		if (ColorEdit("Z Max", directional.z.max))
		{
			wasEdited = true;
		}

		ImGui::PopID();

		return wasEdited;
	}

	bool DirectionalAmbientLightingColorsEditor(const char* label,
		RE::BGSDirectionalAmbientLightingColors& colors)
	{
		ImGui::PushID(label);

		bool wasEdited = false;
		if (DirectionalAmbientLightingColorsDirectionalEditor("##Directional", colors.directional))
		{
			wasEdited = true;
		}
		if (ColorEdit("Specular", colors.specular))
		{
			wasEdited = true;
		}
		if (ImGui::DragFloat("Fresnel Power", &colors.fresnelPower, 0.1f))
		{
			wasEdited = true;
		}

		ImGui::PopID();

		return wasEdited;
	}
}
