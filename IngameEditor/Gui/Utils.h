#pragma once

#include "Gui/ComboWithFilter.h"
#include "Utils/Engine.h"

#include <RE/T/TESDataHandler.h>

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#	define IMGUI_DEFINE_MATH_OPERATORS
#endif  // IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <magic_enum/magic_enum.hpp>

namespace SIE
{
	bool ColorEdit(const char* label, RE::Color& color, ImGuiColorEditFlags flags = 0);
	bool FormEditor(RE::TESForm* form, bool isChanged);
	bool SliderAngleDegree(const char* label, float* v_deg, float v_degrees_min = -360.0f,
		float v_degrees_max = +360.0f, const char* format = "%.0f deg", ImGuiSliderFlags flags = 0);
	bool SliderAngleUInt8(const char* label, uint8_t* v_rad, float v_degrees_min = -360.0f,
		float v_degrees_max = +360.0f, const char* format = "%.0f deg", ImGuiSliderFlags flags = 0);
	bool SliderFloatUInt8(const char* label, uint8_t* v, float v_min, float v_max,
		const char* format = "%.3f", ImGuiSliderFlags flags = 0);
	bool PushingCollapsingHeader(const char* label, ImGuiTreeNodeFlags flags = 0);
	bool ReadOnlyCheckbox(const char* label, bool value);

	template <size_t N>
	void Normalize(float v[N])
	{
		float norm = 0.f;
		for (size_t index = 0; index < N; ++index)
		{
			norm += v[index] * v[index];
		}
		norm = std::sqrtf(norm);
		if (norm != 0.f)
		{
			for (size_t index = 0; index < N; ++index)
			{
				v[index] /= norm;
			}
		}
	}

	template<size_t N>
	bool SliderFloatNormalized(const char* label, float v[N], float v_min, float v_max,
		const char* format = "%.3f", ImGuiSliderFlags flags = 0)
	{
		bool wasEdited = ImGui::SliderScalarN(label, ImGuiDataType_Float, v, static_cast<int>(N), &v_min,
			&v_max, format, flags);
		if (wasEdited)
		{
			Normalize<N>(v);
		}
		return wasEdited;
	}

	template<typename EnumT, typename UnderlyingT>
	bool FlagEdit(const char* label, stl::enumeration<EnumT, UnderlyingT>& flagsValue,
		EnumT flagMask)
	{
		bool isSelected = flagsValue.any(flagMask);
		bool wasEdited = false;
		if (ImGui::Checkbox(label, &isSelected))
		{
			if (isSelected)
			{
				flagsValue.set(flagMask);
			}
			else
			{
				flagsValue.reset(flagMask);
			}
			wasEdited = true;
		}
		return wasEdited;
	}

	bool BSFixedStringEdit(const char* label, RE::BSFixedString& text);
	bool PathEdit(const char* label, const char* text, RE::BSFixedString& path, const char* filter,
		const char* folder);
	bool TexturePathEdit(const char* label, const char* text, RE::BSFixedString& path);
	bool NifTexturePathEdit(const char* label, const char* text, RE::BSFixedString& path);
	bool EspPathEdit(const char* label, const char* text, RE::BSFixedString& path);
	bool MeshPathEdit(const char* label, const char* text, RE::BSFixedString& path);
	bool FreeMeshPathEdit(const char* label, const char* text, RE::BSFixedString& path);

	template <bool NoneAllowed, typename T>
	bool FormSelector(const char* label, T*& currentForm)
	{
		static bool listInitialized = false;

		static RE::BSTArray<T*> forms;
		static std::vector<std::string> items;

		if (!listInitialized)
		{
			const auto dataHandler = RE::TESDataHandler::GetSingleton();
			forms = dataHandler->GetFormArray<T>();
			std::sort(forms.begin(), forms.end(),
				[](const auto& first, const auto& second)
				{ return std::strcmp(first->GetFormEditorID(), second->GetFormEditorID()) < 0; });

			items.reserve(NoneAllowed ? forms.size() + 1 : forms.size());
			if (NoneAllowed)
			{
				items.push_back("NONE");
			}
			for (T* form : forms)
			{
				items.push_back(GetFullName(*form));
			}

			listInitialized = true;
		}

		int selectedIndex = -1;
		if constexpr (NoneAllowed)
		{
			if (currentForm == nullptr)
			{
				selectedIndex = 0;
			}
		}
		if (selectedIndex == -1 && currentForm != nullptr)
		{
			for (uint32_t formIndex = 0; formIndex < forms.size(); ++formIndex)
			{
				if (currentForm == forms[formIndex])
				{
					selectedIndex = static_cast<int>(NoneAllowed ? formIndex + 1 : formIndex);
				}
			}
		}

		if (ImGui::ComboWithFilter(label, &selectedIndex, items, 15))
		{
			currentForm = NoneAllowed ? (selectedIndex == 0 ? nullptr : forms[selectedIndex - 1]) :
                                        forms[selectedIndex];
			return true;
		}

		return false;
	}

	template <typename EnumType, typename ValueType>
	bool EnumSelector(const char* label, ValueType& enumValue)
	{
		bool wasSelected = false;
		if (ImGui::BeginCombo(label,
				magic_enum::enum_name(static_cast<EnumType>(enumValue)).data()))
		{
			for (const auto& [value, name] : magic_enum::enum_entries<EnumType>())
			{
				const bool isSelected = static_cast<ValueType>(value) == enumValue;
				if (ImGui::Selectable(name.data(), isSelected))
				{
					enumValue = static_cast<ValueType>(value);
					wasSelected = true;
				}
			}
			ImGui::EndCombo();
		}
		return wasSelected;
	}

	template <typename EnumType, typename UnderlyingType>
	bool EnumSelector(const char* label, stl::enumeration<EnumType, UnderlyingType>& enumValue)
	{
		bool wasSelected = false;
		auto value = enumValue.get();
		if (EnumSelector<EnumType>(label, value))
		{
			enumValue = value;
			wasSelected = true;
		}
		return wasSelected;
	}

	template<typename FormType, typename CollectionType>
	bool FormListBox(const char* label, CollectionType& items, int& selectedIndex,
		FormType*& addSelectedItem)
	{
		ImGui::PushID(label);

		bool wasEdited = false;

		FormSelector<false>("##ChoiceComboBox", addSelectedItem);
		if (addSelectedItem != nullptr)
		{
			ImGui::SameLine();
			if (ImGui::Button("Add"))
			{
				items.push_back(addSelectedItem);
				wasEdited = true;
			}
		}

		ImGui::BeginListBox("##CurrentListBox");
		for (int itemIndex = 0; itemIndex < items.size(); ++itemIndex)
		{
			if (ImGui::Selectable(std::format("{}##{}", GetFullName(*items[itemIndex]), itemIndex).c_str(),
					selectedIndex == itemIndex))
			{
				selectedIndex = itemIndex;
			}
		}
		ImGui::EndListBox();

		if (selectedIndex >= 0 && selectedIndex < items.size())
		{
			ImGui::SameLine();
			if (ImGui::Button("Remove"))
			{
				items.erase(std::next(items.begin(), selectedIndex));
				wasEdited = true;
			}
		}

		ImGui::PopID();

		return wasEdited;
	}
}
