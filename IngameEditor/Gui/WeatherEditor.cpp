#include "Gui/WeatherEditor.h"

#include "Core/Core.h"
#include "Gui/DirectionalAmbientLightingColorsEditor.h"
#include "Gui/ImageSpaceEditor.h"
#include "Gui/NiObjectEditor.h"
#include "Gui/ShaderParticleEditor.h"
#include "Gui/Utils.h"
#include "Gui/VolumetricLightingEditor.h"
#include "Systems/WeatherEditorSystem.h"
#include "Utils/Engine.h"

#include "3rdparty/ImGuiFileDialog/ImGuiFileDialog.h"

#include <RE/A/Atmosphere.h>
#include <RE/B/BGSReferenceEffect.h>
#include <RE/B/BGSShaderParticleGeometryData.h>
#include <RE/B/BGSSoundDescriptorForm.h>
#include <RE/B/BGSVolumetricLighting.h>
#include <RE/C/Calendar.h>
#include <RE/C/Clouds.h>
#include <RE/M/Moon.h>
#include <RE/N/NiCamera.h>
#include <RE/S/Sky.h>
#include <RE/S/Stars.h>
#include <RE/S/Sun.h>
#include <RE/T/TESImageSpace.h>
#include <RE/T/TESObjectSTAT.h>

#include <ranges>

namespace SIE
{
	namespace SWeatherEditor
	{
		const std::array<const char*, 17> ColorTypeNames{ { "Sky Upper", "Fog Near", "Cloud Layer",
			"Ambient", "Sunlight", "Sun", "Stars", "Sky Lower", "Horizon", "Effect Lighting",
			"Cloud LOD Diffuse", "Cloud LOD Ambient", "Fog Far", "Sky Statics", "Water Multiplier",
			"Sun Glare", "Moon Glare" } };

		const std::array<const char*, 4> ColorTimeNames{ { "Sunrise", "Day", "Sunset", "Night" } };

		const std::unordered_map<uint8_t, const char*> WeatherClassificationNames{
			{ uint8_t(0b0), "None" },
			{ uint8_t(0b1), "Pleasant" }, { uint8_t(0b10), "Cloudy" }, { uint8_t(0b100), "Rainy" }, { uint8_t(0b1000), "Snow" } };

		constexpr uint8_t WeatherClassificationMask = uint8_t(0b1111);

	    const char* GetWeatherClassificationName(uint8_t flags)
	    {
			return WeatherClassificationNames.at(flags & WeatherClassificationMask);
	    }

		RE::TESWeather::WeatherDataFlag GetNewWeatherClassificationFlag(uint8_t flags, uint8_t value)
	    {
			return static_cast<RE::TESWeather::WeatherDataFlag>((flags & ~WeatherClassificationMask) | value);
	    }

		bool Color3Edit(const char* label, RE::TESWeather::Data::Color3& color,
			ImGuiColorEditFlags flags = 0)
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

		bool ResetTimeForColorButton(RE::Sky& sky, RE::TESWeather::ColorTimes::ColorTime colorTime)
		{
			if (ImGui::Button("Reset Time"))
			{
				ResetTimeForColor(sky, colorTime);
				return true;
			}
			return false;
		}

		bool ResetTimeForFogButton(RE::Sky& sky, bool isDay)
		{
			if (ImGui::Button("Reset Time"))
			{
				ResetTimeForFog(sky, isDay);
				return true;
			}
			return false;
		}

		void ColorTimeInfo(RE::Sky& sky) 
		{ 
			sky.currentGameHour = RE::Calendar::GetSingleton()->GetHour();
			const auto colorTimeInfo = sky.GetColorTimeInfo();
			if (colorTimeInfo.needsTimeInterpolation)
			{
				ImGui::Text(std::format("Interpolating between {} and {} with {} fraction {}",
					ColorTimeNames[static_cast<size_t>(colorTimeInfo.firstTime)],
					ColorTimeNames[static_cast<size_t>(colorTimeInfo.secondTime)],
					ColorTimeNames[static_cast<size_t>(colorTimeInfo.firstTime)], colorTimeInfo
						.timePercent)
								.c_str());
			}
			else
			{
				ImGui::Text(std::format("Current time is {}",
					ColorTimeNames[static_cast<size_t>(colorTimeInfo.firstTime)]).c_str());
			}
		}

		void FogTimeInfo(RE::Sky& sky)
		{
			sky.currentGameHour = RE::Calendar::GetSingleton()->GetHour();
			const auto fogTimeInfo = sky.GetFogTimeInfo();
			ImGui::Text(std::format("Day fraction is {}", fogTimeInfo.dayPercent).c_str());
		}

		bool CloudSpeedEdit(const char* label, RE::TESWeather& weather, size_t layerIndex)
		{
			float cloudSpeed[2]{ weather.cloudLayerSpeedX[layerIndex] / 1270.f - 0.1f ,
				weather.cloudLayerSpeedY[layerIndex] / 1270.f - 0.1f };
			const bool result = ImGui::SliderFloat2(label, cloudSpeed, -0.1f, 0.1f);
			if (result)
			{
				weather.cloudLayerSpeedX[layerIndex] =
					static_cast<int8_t>(1270 * (cloudSpeed[0] - -0.1f));
				weather.cloudLayerSpeedY[layerIndex] =
					static_cast<int8_t>(1270 * (cloudSpeed[1] - -0.1f));
			}
			return result;
		}

		bool WeatherClassificationEdit(const char* label, RE::TESWeather& weather)
	    {
			bool wasEdited = false;
			if (ImGui::BeginCombo(label,
					GetWeatherClassificationName(weather.data.flags.underlying())))
			{
				for (const auto& [value, name] : WeatherClassificationNames)
				{
					if (ImGui::Selectable(name, weather.data.flags.underlying() & value))
					{
						weather.data.flags =
							GetNewWeatherClassificationFlag(weather.data.flags.underlying(), value);
						wasEdited = true;
					}
				}
				ImGui::EndCombo();
			}
			return wasEdited;
	    }

		bool CloudLayerCheckbox(const char* label, RE::TESWeather& weather,
			uint8_t layerIndex, bool& isEnabled)
		{
			bool wasEdited = false;
			if (ImGui::Checkbox(label, &isEnabled))
			{
				if (isEnabled)
				{
					weather.cloudLayerDisabledBits &= ~(1 << layerIndex);
				}
				else
				{
					weather.cloudLayerDisabledBits |= (1 << layerIndex);
				}
				wasEdited = true;
			}
			return wasEdited;
		}

		bool SoundsListBox(const char* label, RE::TESWeather& weather)
		{
			std::vector<RE::TESWeather::WeatherSound*> items(weather.sounds.begin(),
				weather.sounds.end());

			static int selectedIndex = -1;
			static RE::BGSSoundDescriptorForm* addSelectedItem = nullptr;

			ImGui::PushID(label);

			bool wasEdited = false;

			FormSelector<false>("##ChoiceComboBox", addSelectedItem);
			if (addSelectedItem != nullptr)
			{
				ImGui::SameLine();
				if (ImGui::Button("Add"))
				{
					items.push_back(new RE::TESWeather::WeatherSound{
						addSelectedItem->formID, RE::TESWeather::SoundType::kDefault });
					wasEdited = true;
				}
			}

			ImGui::BeginListBox("##CurrentListBox");
			for (int currentIndex = 0; currentIndex < items.size(); ++currentIndex)
			{
				if (ImGui::Selectable(std::format("{}##{}",
							GetFullName(*RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(items[currentIndex]->soundFormID)),
										  currentIndex)
										  .c_str(),
						selectedIndex == currentIndex))
				{
					selectedIndex = currentIndex;
				}
			}
			ImGui::EndListBox();

			if (selectedIndex >= 0 && selectedIndex < items.size())
			{
				ImGui::SameLine();
				bool needRemove = false;
				if (ImGui::Button("Remove"))
				{
					needRemove = true;
				}
				if (EnumSelector("Type", items[selectedIndex]->type))
				{
					wasEdited = true;
				}
				if (needRemove)
				{
					items.erase(std::next(items.begin(), selectedIndex));
					wasEdited = true;
				}
			}
			else
			{
				selectedIndex = -1;
			}

			weather.sounds = {};
			for (auto& item : std::ranges::reverse_view(items))
            {
				weather.sounds.push_front(item);
			}

			ImGui::PopID();

			return wasEdited;
		}

		void CloudHelper(const RE::Clouds& clouds) 
		{ 
			auto playerCamera = static_cast<RE::NiCamera*>(
				RE::PlayerCamera::GetSingleton()->cameraRoot->children[0].get());
			auto& io = ImGui::GetIO();

			size_t index = 0;
			for (const auto& child : clouds.root->children)
			{
				child->UpdateWorldBound();
				const auto& center = child->worldBound.center;
				float screenX, screenY, screenZ;
				if (RE::NiCamera::WorldPtToScreenPt3(playerCamera->worldToCam, playerCamera->port, center, screenX,
					screenY, screenZ, 0.f) && screenX >= 0.f, screenY >= 0.f && screenX <= 1.f && screenY <= 1.f)
				{
					screenX *= io.DisplaySize.x;
					screenY *= io.DisplaySize.y;
					ImGui::SetNextWindowPos(ImVec2(screenX, screenY));
					ImGui::SetNextWindowSize(ImVec2(20.f, 20.f));
					constexpr ImGuiWindowFlags invisibleWindowFlag =
						ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration |
						ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs |
						ImGuiWindowFlags_NoSavedSettings;
					const std::string hint = std::to_string(index);
					ImGui::Begin(hint.c_str(), nullptr, invisibleWindowFlag);
					ImGui::Text(hint.c_str());
					ImGui::End();
				}
				++index;
			}
		}
	}

	void WeatherEditor(const char* label, RE::TESWeather& weather)
	{
		using namespace SWeatherEditor;

		auto& sky = *RE::Sky::GetSingleton();

		ImGui::PushID(label);

		auto& core = Core::GetInstance();
		if (auto weatherEditorSystem = core.GetSystem<WeatherEditorSystem>())
		{
			bool isLocked = weatherEditorSystem->IsWeatherLocked();
			if (ImGui::Checkbox("Lock", &isLocked))
			{
				weatherEditorSystem->LockWeather(isLocked);
			}
		}

		bool needsReset = false;

		const auto nonResetEditor = [&weather](bool isEdited)
		{
		    return FormEditor(&weather, isEdited);
		};
		const auto resetEditor = [&needsReset, &nonResetEditor](bool isEdited)
		{
			if (nonResetEditor(isEdited))
			{
				needsReset = true;
			}
			return isEdited;
		};

		if (PushingCollapsingHeader("General"))
		{
			if (PushingCollapsingHeader("Imagespaces"))
			{
				ColorTimeInfo(sky);
				for (size_t colorTime = 0; colorTime < RE::TESWeather::ColorTime::kTotal;
					 ++colorTime)
				{
                    const auto currentImageSpace = weather.imageSpaces[colorTime];
					if (currentImageSpace != nullptr &&
						PushingCollapsingHeader(ColorTimeNames[colorTime]))
					{
						ResetTimeForColorButton(
							sky,
							static_cast<RE::TESWeather::ColorTimes::ColorTime>(colorTime));
						nonResetEditor(FormSelector<false>("Current imagespace",
							weather.imageSpaces[colorTime]));
						ImageSpaceEditor("ImageSpaceEditor", *currentImageSpace);
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
			if (PushingCollapsingHeader("Volumetric Lighting"))
			{
				ColorTimeInfo(sky);
				for (size_t colorTime = 0; colorTime < RE::TESWeather::ColorTime::kTotal;
					 ++colorTime)
				{
					if (PushingCollapsingHeader(ColorTimeNames[colorTime]))
					{
						ResetTimeForColorButton(sky,
							static_cast<RE::TESWeather::ColorTimes::ColorTime>(colorTime));
						nonResetEditor(FormSelector<true>("Current lighting",
												 weather.volumetricLighting[colorTime]));
						if (weather.volumetricLighting[colorTime] != nullptr)
						{
							VolumetricLightingEditor("VOLIEditor",
								*weather.volumetricLighting[colorTime]);
						}
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
			if (PushingCollapsingHeader("Fog"))
			{
				FogTimeInfo(sky);
				if (PushingCollapsingHeader("Day"))
				{
					ResetTimeForFogButton(sky, true);
					nonResetEditor(ImGui::DragFloat("Near", &weather.fogData.dayNear, 50.f));
					nonResetEditor(ImGui::DragFloat("Far", &weather.fogData.dayFar, 50.f));
					nonResetEditor(ImGui::DragFloat("Power", &weather.fogData.dayPower, 0.1f, 0.f));
					nonResetEditor(ImGui::DragFloat("Max", &weather.fogData.dayMax, 0.1f));
					ImGui::TreePop();
				}
				if (PushingCollapsingHeader("Night"))
				{
					ResetTimeForFogButton(sky, false);
					nonResetEditor(ImGui::DragFloat("Near", &weather.fogData.nightNear, 50.f));
					nonResetEditor(ImGui::DragFloat("Far", &weather.fogData.nightFar, 50.f));
					nonResetEditor(
						ImGui::DragFloat("Power", &weather.fogData.nightPower, 0.1f, 0.f));
					nonResetEditor(ImGui::DragFloat("Max", &weather.fogData.nightMax, 0.1f));
					ImGui::TreePop();
				}
				ImGui::TreePop();
			}
			if (PushingCollapsingHeader("Colors"))
			{
				ColorTimeInfo(sky);
				for (size_t colorTime = 0; colorTime < RE::TESWeather::ColorTime::kTotal;
					 ++colorTime)
				{
					if (PushingCollapsingHeader(ColorTimeNames[colorTime]))
					{
						ResetTimeForColorButton(sky, 
							static_cast<RE::TESWeather::ColorTimes::ColorTime>(colorTime));
						for (size_t colorType = 0; colorType < RE::TESWeather::ColorTypes::kTotal;
							 ++colorType)
						{
							nonResetEditor(ColorEdit(ColorTypeNames[colorType],
								weather.colorData[colorType][colorTime]));
						}
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
			if (PushingCollapsingHeader("Cloud Textures"))
			{
				//SWeatherEditor::CloudHelper(*sky.clouds);
				uint8_t layerIndex = 0;
				for (const auto& child : sky.clouds->root->children)
				{
					if (PushingCollapsingHeader(child->name.c_str()))
					{
						bool isEnabled = !((weather.cloudLayerDisabledBits >> layerIndex) & 1);
						resetEditor(CloudLayerCheckbox("Enabled", weather, layerIndex, isEnabled));
						if (isEnabled)
						{
							resetEditor(TexturePathEdit("TexturePath", "Path",
								weather.cloudTextures[layerIndex].textureName));
							resetEditor(CloudSpeedEdit("Cloud Speed", weather, layerIndex));
							ColorTimeInfo(sky);
							for (size_t colorTime = 0;
								 colorTime < RE::TESWeather::ColorTime::kTotal; ++colorTime)
							{
								if (PushingCollapsingHeader(ColorTimeNames[colorTime]))
								{
									ResetTimeForColorButton(sky, 
										static_cast<RE::TESWeather::ColorTimes::ColorTime>(
											colorTime));
									nonResetEditor(ColorEdit("Color",
										weather.cloudColorData[layerIndex][colorTime]));
									nonResetEditor(ImGui::DragFloat("Alpha",
										&weather.cloudAlpha[layerIndex][colorTime], 0.05f, 0.f,
										1.f));
									ImGui::TreePop();
								}
							}
						}
						ImGui::TreePop();
					}
					++layerIndex;
				}
				ImGui::TreePop();
			}
			if (PushingCollapsingHeader("Wind"))
			{
				nonResetEditor(
					SliderAngleUInt8("Wind Direction", &weather.data.windDirection, 0.f, 360.f));
				nonResetEditor(SliderAngleUInt8("Wind Direction Range",
										 &weather.data.windDirectionRange, 0.f, 180.f));
				nonResetEditor(SliderFloatUInt8("Wind Speed", &weather.data.windSpeed, 0.f, 1.f));
				nonResetEditor(
					SliderFloatUInt8("Trans Delta", &weather.data.transDelta, 0.f, 0.25f));
				nonResetEditor(SliderFloatUInt8("Sun Glare", &weather.data.sunGlare, 0.f, 1.f));
				nonResetEditor(SliderFloatUInt8("Sun Damage", &weather.data.sunDamage, 0.f, 1.f));
				ImGui::TreePop();
			}
			ImGui::TreePop();
		}
		if (PushingCollapsingHeader("Directional Ambient"))
		{
			ColorTimeInfo(sky);
			for (size_t colorTime = 0; colorTime < RE::TESWeather::ColorTime::kTotal; ++colorTime)
			{
				if (PushingCollapsingHeader(ColorTimeNames[colorTime]))
				{
					ResetTimeForColorButton(sky, static_cast<RE::TESWeather::ColorTimes::ColorTime>(colorTime));
					nonResetEditor(DirectionalAmbientLightingColorsEditor("##DALC",
						weather.directionalAmbientLightingColors[colorTime]));
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
		if (PushingCollapsingHeader("Precipitation"))
		{
			if (PushingCollapsingHeader("Precipitation"))
			{
				resetEditor(
					FormSelector<true>("Shader Particle", weather.precipitationData));
				if (weather.precipitationData != nullptr)
				{
					if (ShaderParticleEditor("SPGDEditor", *weather.precipitationData))
					{
						needsReset = true;
					}
				}
				resetEditor(SliderFloatUInt8("Begin Fade In",
					&weather.data.precipitationBeginFadeIn, 0.f, 1.f));
				resetEditor(SliderFloatUInt8("End Fade Out", &weather.data.precipitationEndFadeOut,
					0.f, 1.f));
				ImGui::TreePop();
			}
			if (PushingCollapsingHeader("Thunder/Lighting"))
			{
				resetEditor(SliderFloatUInt8("Begin Fade In",
					&weather.data.thunderLightningBeginFadeIn, 0.f, 1.f));
				resetEditor(SliderFloatUInt8("End Fade Out",
					&weather.data.thunderLightningEndFadeOut, 0.f, 1.f));
				{
					constexpr uint8_t minFrequency = 15;
					constexpr uint8_t maxFrequency = 255;
					resetEditor(ImGui::SliderScalarN("Infrequency", ImGuiDataType_U8,
						&weather.data.thunderLightningFrequency, 1, &minFrequency, &maxFrequency));
				}
				resetEditor(Color3Edit("Lightning Color", weather.data.lightningColor));
				ImGui::TreePop();
			}
			resetEditor(WeatherClassificationEdit("Weather Classification", weather));
			ImGui::TreePop();
		}
		if (PushingCollapsingHeader("Sound"))
		{
			resetEditor(SoundsListBox("##SoundsListBox", weather));
			ImGui::TreePop();
		}
		if (PushingCollapsingHeader("Effects"))
		{
			if (PushingCollapsingHeader("Visual Effect"))
			{
				resetEditor(FormSelector<true>("Visual Effect", weather.referenceEffect));
				resetEditor(
					SliderFloatUInt8("Begin Effect", &weather.data.visualEffectBegin, 0.f, 1.f));
				resetEditor(
					SliderFloatUInt8("End Effect", &weather.data.visualEffectEnd, 0.f, 1.f));
				ImGui::TreePop();
			}
			if (PushingCollapsingHeader("Sky Statics"))
			{
				static int selectedIndex = -1;
				static RE::TESObjectSTAT* addSelectedItem = nullptr;
				resetEditor(
					FormListBox("SkyStatics", weather.skyStatics, selectedIndex, addSelectedItem));
				ImGui::TreePop();
			}
			if (PushingCollapsingHeader("Aurora"))
			{
				resetEditor(MeshPathEdit("AuroraPath", "Path", weather.aurora.model));
				resetEditor(FlagEdit("Always Visible", weather.data.flags,
					RE::TESWeather::WeatherDataFlag::kPermAurora));
				resetEditor(FlagEdit("Follows Sun Position", weather.data.flags,
					RE::TESWeather::WeatherDataFlag::kAuroraFollowsSun));
				ImGui::TreePop();
			}
			ImGui::TreePop();
		}
		if (PushingCollapsingHeader("3D"))
		{
			if (PushingCollapsingHeader("Atmosphere"))
			{
				if (DispatchableNiObjectEditor("", *sky.atmosphere->root))
				{
					RE::NiUpdateData updateData;
					updateData.flags.set(RE::NiUpdateData::Flag::kDirty);
					sky.atmosphere->root->Update(updateData);
				}
				ImGui::TreePop();
			}
			if (PushingCollapsingHeader("Stars"))
			{
				if (DispatchableNiObjectEditor("", *sky.stars->root))
				{
					RE::NiUpdateData updateData;
					updateData.flags.set(RE::NiUpdateData::Flag::kDirty);
					sky.stars->root->Update(updateData);
				}
				ImGui::TreePop();
			}
			if (PushingCollapsingHeader("Sun"))
			{
				if (DispatchableNiObjectEditor("", *sky.sun->root))
				{
					RE::NiUpdateData updateData;
					updateData.flags.set(RE::NiUpdateData::Flag::kDirty);
					sky.sun->root->Update(updateData);
				}
				ImGui::TreePop();
			}
			if (PushingCollapsingHeader("Clouds"))
			{
				if (DispatchableNiObjectEditor("", *sky.clouds->root))
				{
					RE::NiUpdateData updateData;
					updateData.flags.set(RE::NiUpdateData::Flag::kDirty);
					sky.clouds->root->Update(updateData);
				}
				ImGui::TreePop();
			}
			if (PushingCollapsingHeader("Masser"))
			{
				if (DispatchableNiObjectEditor("", *sky.masser->root))
				{
					RE::NiUpdateData updateData;
					updateData.flags.set(RE::NiUpdateData::Flag::kDirty);
					sky.masser->root->Update(updateData);
				}
				ImGui::TreePop();
			}
			if (PushingCollapsingHeader("Secunda"))
			{
				if (DispatchableNiObjectEditor("", *sky.secunda->root))
				{
					RE::NiUpdateData updateData;
					updateData.flags.set(RE::NiUpdateData::Flag::kDirty);
					sky.secunda->root->Update(updateData);
				}
				ImGui::TreePop();
			}
			ImGui::TreePop();
		}

		if (needsReset)
		{
			const auto sky = RE::Sky::GetSingleton();
			sky->ForceWeather(&weather, true);
		}

		ImGui::PopID();
	}
}
