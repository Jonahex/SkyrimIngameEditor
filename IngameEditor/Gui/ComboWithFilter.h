#pragma once

#include <vector>

namespace ImGui
{
	bool ComboWithFilter(const char* label, int* currentItem,
		const std::vector<std::string>& items, int popupMaxHeightInItems = -1);
}
