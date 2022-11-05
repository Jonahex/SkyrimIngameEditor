#include "Gui/Utils.h"

#include "Serialization/Serializer.h"
#include "Utils/Engine.h"

#include "3rdparty/ImGuiFileDialog/ImGuiFileDialog.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

#define _USE_MATH_DEFINES

#include <math.h>

constexpr float RadToDeg = static_cast<float>(180 / M_PI);

namespace SIE
{
	bool ColorEdit(const char* label, RE::Color& color, ImGuiColorEditFlags flags)
	{
		float col[3];
		col[0] = U8ToFloatColor(color.red);
		col[1] = U8ToFloatColor(color.green);
		col[2] = U8ToFloatColor(color.blue);

		const bool result = ImGui::ColorEdit3(label, col, flags);

		color.red = FloatToU8Color(col[0]);
		color.green = FloatToU8Color(col[1]);
		color.blue = FloatToU8Color(col[2]);

		return result;
	}

	bool FormEditor(RE::TESForm* form, bool isChanged)
	{
		if (isChanged)
		{
			Serializer::Instance().EnqueueForm(*form);
		}
		return isChanged;
	}

	bool SliderAngleDegree(const char* label, float* v_deg, float v_degrees_min,
		float v_degrees_max, const char* format, ImGuiSliderFlags flags)
	{
		float value = *v_deg / RadToDeg;

		const bool result =
			ImGui::SliderAngle(label, &value, v_degrees_min, v_degrees_max, format, flags);

		*v_deg = value * RadToDeg;

		return result;
	}

	bool SliderAngleUInt8(const char* label, uint8_t* v_rad, float v_degrees_min,
		float v_degrees_max, const char* format, ImGuiSliderFlags flags)
	{
		float value = (*v_rad / 255.f * (v_degrees_max - v_degrees_min) + v_degrees_min) / RadToDeg;

		const bool result =
			ImGui::SliderAngle(label, &value, v_degrees_min, v_degrees_max, format, flags);

		*v_rad = static_cast<uint8_t>(
			(value * RadToDeg - v_degrees_min) / (v_degrees_max - v_degrees_min) * 255.f);

		return result;
	}

	bool SliderFloatUInt8(const char* label, uint8_t* v, float v_min, float v_max,
		const char* format, ImGuiSliderFlags flags)
	{
		float value = *v / 255.f * (v_max - v_min) + v_min;

		const bool result = ImGui::SliderFloat(label, &value, v_min, v_max, format, flags);

		*v = static_cast<uint8_t>((value - v_min) / (v_max - v_min) * 255.f);

		return result;
	}

	bool PushingCollapsingHeader(const char* label, ImGuiTreeNodeFlags flags)
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return false;

		return ImGui::TreeNodeBehavior(window->GetID(label), flags, label);
	}

	bool PathEdit(const char* label, const char* text, RE::BSFixedString& path, const char* filter,
		const char* folder)
	{
		ImGui::PushID(label);

		bool result = false;

		std::string texturePath = path.c_str();
		if (ImGui::InputText("##TextEdit", &texturePath))
		{
			path = texturePath;
			result = true;
		}

		ImGui::SameLine();

		if (ImGui::Button("Browse"))
		{
			ImGuiFileDialog::Instance()->OpenDialog(std::to_string(ImGui::GetID(label)),
				"Choose File", filter, folder);
		}

		if (ImGuiFileDialog::Instance()->Display(std::to_string(ImGui::GetID(label))))
		{
			if (ImGuiFileDialog::Instance()->IsOk())
			{
				const auto relPath =
					std::filesystem::path(ImGuiFileDialog::Instance()->GetFilePathName())
						.lexically_relative(std::filesystem::current_path().append(folder))
						.native();
				path = std::string(relPath.cbegin(), relPath.cend());
				result = true;
			}

			ImGuiFileDialog::Instance()->Close();
		}

		ImGui::SameLine();
		if (*text != '\0')
		{
			ImGui::Text(text);
		}

		ImGui::PopID();

		return result;
	}

	bool TexturePathEdit(const char* label, const char* text, RE::BSFixedString& path)
	{
		return PathEdit(label, text, path, ".dds", "Data\\Textures");
	}

	bool EspPathEdit(const char* label, const char* text, RE::BSFixedString& path)
	{
		return PathEdit(label, text, path, ".esp", "Data");
	}

	bool MeshPathEdit(const char* label, const char* text, RE::BSFixedString& path)
	{
		return PathEdit(label, text, path, ".nif", "Data\\Meshes");
	}
}
