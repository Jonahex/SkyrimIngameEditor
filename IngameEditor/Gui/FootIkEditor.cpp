#include "Gui/FootIkEditor.h"

#include "Gui/Utils.h"

#include <RE/H/hkbCharacter.h>
#include <RE/H/hkbCharacterData.h>
#include <RE/H/hkbCharacterSetup.h>
#include <RE/H/hkbFootIkDriver.h>
#include <RE/H/hkbFootIkDriverInfo.h>
#include <RE/H/hkbFootIkDriverInfoLeg.h>

namespace SIE
{
	bool FootIkEditor(const char* label, RE::hkbCharacter& character)
	{
		bool wasEdited = false;

		if (const auto& characterSetup = character.setup)
		{
			if (const auto& characterData = characterSetup->m_data)
			{
				if (const auto& footIk = characterData->m_footIkDriverInfo)
				{
					if (PushingCollapsingHeader("Foot IK"))
					{
						size_t legIndex = 0;
						for (auto& leg : footIk->m_legs)
						{
							if (PushingCollapsingHeader(std::format("Leg {}", legIndex).c_str()))
							{
								if (SliderFloatNormalized<4>("Knee Axis",
										reinterpret_cast<float*>(&leg.m_kneeAxisLS), -1.f, 1.f))
								{
									wasEdited = true;
								}
								if (SliderFloatNormalized<4>("Foot End",
										reinterpret_cast<float*>(&leg.m_footEndLS), -1.f, 1.f))
								{
									wasEdited = true;
								}
								if (ImGui::SliderFloat("Planted Ankle Height",
										&leg.m_footPlantedAnkleHeightMS, -100.f, 100.f))
								{
									wasEdited = true;
								}
								if (ImGui::SliderFloat("Raised Ankle Height",
										&leg.m_footRaisedAnkleHeightMS, -100.f, 100.f))
								{
									wasEdited = true;
								}
								if (ImGui::SliderFloat("Max Ankle Height", &leg.m_maxAnkleHeightMS,
										-200.f, 200.f))
								{
									wasEdited = true;
								}
								if (ImGui::SliderFloat("Min Ankle Height", &leg.m_minAnkleHeightMS,
										-200.f, 200.f))
								{
									wasEdited = true;
								}
								if (SliderAngleDegree("Min Knee Angle", &leg.m_minKneeAngleDegrees,
										-180, 180))
								{
									wasEdited = true;
								}
								if (SliderAngleDegree("Max Knee Angle", &leg.m_maxKneeAngleDegrees,
										-180, 180))
								{
									wasEdited = true;
								}
								if (SliderAngleDegree("Max Ankle Angle",
										&leg.m_maxAnkleAngleDegrees, -180, 180))
								{
									wasEdited = true;
								}
								ImGui::TreePop();
							}
							legIndex++;
						}

						ImGui::TreePop();
					}
				}
			}
		}

		if (wasEdited)
		{
			character.footIkDriver->m_isSetUp = false;
		}

		return wasEdited;
	}
}
