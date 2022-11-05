// Posted in issue: https://github.com/ocornut/imgui/issues/1658#issuecomment-1086193100

#define FTS_FUZZY_MATCH_IMPLEMENTATION
#define IMGUI_DEFINE_MATH_OPERATORS

#include "Gui/ComboWithFilter.h"

#include "3rdparty/fts_fuzzy_match.h" // https://github.com/forrestthewoods/lib_fts/blob/632ca1ea82bdf65688241bb8788c77cb242fba4f/code/fts_fuzzy_match.h

#include <imgui.h>
#include <imgui_internal.h>

namespace ImGui
{
	static int IndexOfKey(std::vector<std::pair<int, int>> pairList, int key)
	{
		for (int i = 0; i < pairList.size(); ++i)
		{
			auto& p = pairList[i];
			if (p.first == key)
			{
				return i;
			}
		}
		return -1;
	}

	// Copied from imgui_widgets.cpp
	static float CalcMaxPopupHeightFromItemCount(int itemsCount)
	{
		ImGuiContext& g = *GImGui;
		if (itemsCount <= 0)
		{
			return FLT_MAX;
		}
		return (g.FontSize + g.Style.ItemSpacing.y) * itemsCount - g.Style.ItemSpacing.y +
		       (g.Style.WindowPadding.y * 2);
	}

	bool ComboWithFilter(const char* label, int* currentItem,
		const std::vector<std::string>& items, int popupMaxHeightInItems /*= -1 */)
	{
		using namespace fts;

		ImGuiContext& g = *GImGui;

		ImGuiWindow* window = GetCurrentWindow();
		if (window->SkipItems)
		{
			return false;
		}

		int itemsCount = static_cast<int>(items.size());

		// Use imgui Items_ getters to support more input formats.
		const char* previewValue = NULL;
		if (*currentItem >= 0 && *currentItem < itemsCount)
		{
			previewValue = items[*currentItem].c_str();
		}

		static int focusIndex = -1;
		static char patternBuffer[256] = { 0 };

		bool valueChanged = false;

		const ImGuiID id = window->GetID(label);
		const ImGuiID popupId
			= ImHashStr("##ComboPopup", 0, id);  // copied from BeginCombo
		const bool isAlreadyOpen = IsPopupOpen(popupId, ImGuiPopupFlags_None);
		const bool isFiltering = isAlreadyOpen && patternBuffer[0] != '\0';

		int showCount = itemsCount;

		std::vector<std::pair<int, int>> itemScoreVector;
		if (isFiltering)
		{
			// Filter before opening to ensure we show the correct size window.
			// We won't get in here unless the popup is open.
			for (int i = 0; i < itemsCount; i++)
			{
				int score = 0;
				bool matched = fuzzy_match(patternBuffer, items[i].c_str(), score);
				if (matched)
				{
					itemScoreVector.push_back(std::make_pair(i, score));
				}
			}
			std::sort(itemScoreVector.begin(), itemScoreVector.end(),
				[](const std::pair<int, int>& a, const std::pair<int, int>& b)
				{ return (b.second < a.second); });
			int currentScoreIndex = IndexOfKey(itemScoreVector, focusIndex);
			if (currentScoreIndex < 0 && !itemScoreVector.empty())
			{
				focusIndex = itemScoreVector[0].first;
			}
			showCount = static_cast<int>(itemScoreVector.size());
		}

		// Define the height to ensure our size calculation is valid.
		if (popupMaxHeightInItems == -1)
		{
			popupMaxHeightInItems = 5;
		}
		popupMaxHeightInItems = ImMin(popupMaxHeightInItems, showCount);

		if (!(g.NextWindowData.Flags & ImGuiNextWindowDataFlags_HasSizeConstraint))
		{
			SetNextWindowSizeConstraints(ImVec2(0, 0),
				ImVec2(FLT_MAX, CalcMaxPopupHeightFromItemCount(popupMaxHeightInItems + 3)));
		}

		if (!BeginCombo(label, previewValue, ImGuiComboFlags_None))
			return false;

		if (!isAlreadyOpen)
		{
			focusIndex = *currentItem;
			memset(patternBuffer, 0, IM_ARRAYSIZE(patternBuffer));
		}

		ImGui::PushStyleColor(ImGuiCol_FrameBg, (ImVec4)ImColor(240, 240, 240, 255));
		ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(0, 0, 0, 255));
		ImGui::PushItemWidth(-FLT_MIN);
		// Filter input
		if (!isAlreadyOpen)
			ImGui::SetKeyboardFocusHere();
		InputText("##ComboWithFilter_inputText", patternBuffer, 256,
			ImGuiInputTextFlags_AutoSelectAll);

		ImGui::PopStyleColor(2);

		int move_delta = 0;
		if (IsKeyPressed(ImGuiKey_UpArrow))
		{
			--move_delta;
		}
		else if (IsKeyPressed(ImGuiKey_DownArrow))
		{
			++move_delta;
		}
		else if (IsKeyPressed(ImGuiKey_PageUp))
		{
			move_delta -= popupMaxHeightInItems;
		}
		else if (IsKeyPressed(ImGuiKey_PageDown))
		{
			move_delta += popupMaxHeightInItems;
		}

		if (move_delta != 0)
		{
			if (isFiltering)
			{
				int currentScoreIndex = IndexOfKey(itemScoreVector, focusIndex);
				if (currentScoreIndex >= 0)
				{
					const int count = static_cast<int>(itemScoreVector.size());
					currentScoreIndex = ImClamp(currentScoreIndex + move_delta, 0, count - 1);
					focusIndex = itemScoreVector[currentScoreIndex].first;
				}
			}
			else
			{
				focusIndex = ImClamp(focusIndex + move_delta, 0, itemsCount - 1);
			}
		}

		// Copied from ListBoxHeader
		// If popup_max_height_in_items == -1, default height is maximum 7.
		float heightInItems =
			(popupMaxHeightInItems < 0 ? ImMin(itemsCount, 7) : popupMaxHeightInItems) +
			0.25f;
		ImVec2 size;
		size.x = 0.0f;
		size.y = GetTextLineHeightWithSpacing() * heightInItems + g.Style.FramePadding.y * 2.0f;

		if (ImGui::BeginListBox("##ComboWithFilter_itemList", size))
		{
			for (int i = 0; i < showCount; i++)
			{
				int idx = isFiltering ? itemScoreVector[i].first : i;
				PushID((void*)(intptr_t)idx);
				const bool itemSelected = (idx == focusIndex);
				const char* itemText = items[idx].c_str();
				if (Selectable(itemText, itemSelected))
				{
					valueChanged = true;
					*currentItem = idx;
					CloseCurrentPopup();
				}

				if (itemSelected)
				{
					SetItemDefaultFocus();
					// SetItemDefaultFocus doesn't work so also check IsWindowAppearing.
					if (move_delta != 0 || IsWindowAppearing())
					{
						SetScrollHereY();
					}
				}
				PopID();
			}
			ImGui::EndListBox();

			if (IsKeyPressed(ImGuiKey_Enter))
			{
				valueChanged = true;
				*currentItem = focusIndex;
				CloseCurrentPopup();
			}
			else if (IsKeyPressed(ImGuiKey_Escape))
			{
				valueChanged = false;
				CloseCurrentPopup();
			}
		}
		ImGui::PopItemWidth();
		ImGui::EndCombo();

		if (valueChanged)
			MarkItemEdited(g.LastItemData.ID);

		return valueChanged;
	}

}  // namespace ImGui
