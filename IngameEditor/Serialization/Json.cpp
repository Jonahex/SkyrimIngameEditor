#include "Serialization/Json.h"

#include "Serialization/SerializationUtils.h"
#include "Utils/Engine.h"

#include <RE/B/BGSLightingTemplate.h>
#include <RE/B/BGSMaterialType.h>
#include <RE/B/BGSReferenceEffect.h>
#include <RE/B/BGSShaderParticleGeometryData.h>
#include <RE/B/BGSSoundDescriptorForm.h>
#include <RE/B/BGSVolumetricLighting.h>
#include <RE/E/ExtraCellImageSpace.h>
#include <RE/E/ExtraCellSkyRegion.h>
#include <RE/E/ExtraCellWaterEnvMap.h>
#include <RE/E/ExtraCellWaterType.h>
#include <RE/S/SpellItem.h>
#include <RE/T/TESImageSpace.h>
#include <RE/T/TESObjectCELL.h>
#include <RE/T/TESObjectSTAT.h>
#include <RE/T/TESRegion.h>
#include <RE/T/TESWaterForm.h>
#include <RE/T/TESWeather.h>

namespace RE
{
	using namespace nlohmann;

	void to_json(json& j, const Color& color)
	{
		j = json(std::format("{}, {}, {}, {}", color.alpha, color.red, color.green, color.blue));
	}

	void to_json(json& j, const BGSDirectionalAmbientLightingColors& dalc)
	{
		j = json{ { "DirectionalXPlus", dalc.directional.x.max },
			{ "DirectionalXMinus", dalc.directional.x.min },
			{ "DirectionalYPlus", dalc.directional.y.max },
			{ "DirectionalYMinus", dalc.directional.y.min },
			{ "DirectionalZPlus", dalc.directional.z.max },
			{ "DirectionalZMinus", dalc.directional.z.min }, { "Specular", dalc.specular },
			{ "Scale", dalc.fresnelPower } };
	}

	void to_json(json& j, const ImageSpaceBaseData::HDR& hdr)
	{
		j = json{ { "EyeAdaptSpeed", hdr.eyeAdaptSpeed },
			{ "BloomBlurRadius", hdr.bloomBlurRadius }, { "BloomThreshold", hdr.bloomThreshold },
			{ "BloomScale", hdr.bloomScale },
			{ "ReceiveBloomThreshold", hdr.receiveBloomThreshold }, { "White", hdr.white },
			{ "SunlightScale", hdr.sunlightScale }, { "SkyScale", hdr.skyScale },
			{ "EyeAdaptStrength", hdr.eyeAdaptStrength } };
	}

	void to_json(json& j, const ImageSpaceBaseData::Cinematic& cinematic)
	{
		j = json{ { "Saturation", cinematic.saturation }, { "Brightness", cinematic.brightness },
			{ "Contrast", cinematic.contrast } };
	}

	void to_json(json& j, const ImageSpaceBaseData::Tint& tint)
	{
		j = json{ { "Amount", tint.amount },
			{ "Color", std::format("255, {}, {}, {}", SIE::FloatToU8Color(tint.color.red), SIE::FloatToU8Color(tint.color.green),
						   SIE::FloatToU8Color(tint.color.blue)) } };
	}

	void to_json(json& j, const ImageSpaceBaseData::DepthOfField& dof)
	{
		constexpr auto hasSky = [](const ImageSpaceBaseData::DepthOfField::SkyBlurRadius& value)
		{
			using enum ImageSpaceBaseData::DepthOfField::SkyBlurRadius;

			return value == kRadius0 || value == kRadius1 || value == kRadius2 ||
			       value == kRadius3 || value == kRadius4 || value == kRadius5 ||
			       value == kRadius6 || value == kRadius7;
		};

		constexpr auto blurRadius = [](const ImageSpaceBaseData::DepthOfField::SkyBlurRadius& value)
		{
			using enum ImageSpaceBaseData::DepthOfField::SkyBlurRadius;

			if (value == kRadius0 || value == kNoSky_Radius0)
			{
				return 0;
			}
			if (value == kRadius1 || value == kNoSky_Radius1)
			{
				return 1;
			}
			if (value == kRadius2 || value == kNoSky_Radius2)
			{
				return 2;
			}
			if (value == kRadius3 || value == kNoSky_Radius3)
			{
				return 3;
			}
			if (value == kRadius4 || value == kNoSky_Radius4)
			{
				return 4;
			}
			if (value == kRadius5 || value == kNoSky_Radius5)
			{
				return 5;
			}
			if (value == kRadius6 || value == kNoSky_Radius6)
			{
				return 6;
			}
			return 7;
		};

		j = json{ { "Strength", dof.strength }, { "Distance", dof.distance },
			{ "Range", dof.range }, { "BlurRadius", blurRadius(dof.skyBlurRadius.get()) },
			{ "Sky", hasSky(dof.skyBlurRadius.get()) } };
	}

	void to_json(json& j, const TESImageSpace& imageSpace)
	{
		j = json{ { "Hdr", imageSpace.data.hdr }, { "Cinematic", imageSpace.data.cinematic },
			{ "Tint", imageSpace.data.tint }, { "DepthOfField", imageSpace.data.depthOfField } };
	}

	void to_json(json& j, const TESWeather& weather)
	{
		using enum TESWeather::ColorTime;

		constexpr auto saveColorData = [](const auto& colors)
		{
		    using enum TESWeather::ColorTime; 
			return json{ { "Sunrise", colors[kSunrise] }, { "Day", colors[kDay] },
				{ "Sunset", colors[kSunset] }, { "Night", colors[kNight] } };
		};

		constexpr auto saveImageSpaces = [](const auto& imageSpaces)
		{
		    using enum TESWeather::ColorTime;
			return json{ { "Sunrise", SIE::ToFormKey(imageSpaces[kSunrise]) },
				{ "Day", SIE::ToFormKey(imageSpaces[kDay]) },
				{ "Sunset", SIE::ToFormKey(imageSpaces[kSunset]) },
				{ "Night", SIE::ToFormKey(imageSpaces[kNight]) } };
		};

		constexpr auto saveCloudTextures = [](const auto& cloudTextures)
		{
			json j;
			for (const auto& texture : cloudTextures)
			{
				if (texture.textureName.empty())
				{
					j.push_back(nullptr);
				}
				else
				{
					j.push_back(texture.textureName.c_str());
				}
			}
			return j;
		};

		auto saveClouds = [&weather, &saveColorData]()
		{
			json j;
			for (size_t layerIndex = 0; layerIndex < RE::TESWeather::kTotalLayers; ++layerIndex)
			{
				j.push_back({ { "Enabled", !((weather.cloudLayerDisabledBits >> layerIndex) & 1) },
					{ "XSpeed", weather.cloudLayerSpeedX[layerIndex] / 1270.f - 0.1f },
					{ "YSpeed", weather.cloudLayerSpeedY[layerIndex] / 1270.f - 0.1f },
					{ "Colors", saveColorData(weather.cloudColorData[layerIndex]) },
					{ "Alphas", { { "Sunrise", weather.cloudAlpha[layerIndex][kSunrise] },
									{ "Day", weather.cloudAlpha[layerIndex][kDay] },
									{ "Sunset", weather.cloudAlpha[layerIndex][kSunset] },
									{ "Night", weather.cloudAlpha[layerIndex][kNight] } } } });
			};
			return j;
		};

		auto saveSkyStatics = [&weather]()
		{
			json j = json::array();
			for (const auto& item : weather.skyStatics) { j.push_back(SIE::ToFormKey(item)); }
			return j;
		};

		auto saveSounds = [&weather]()
		{
			json j = json::array();
			for (const auto& item : weather.sounds)
			{
				j.push_back({ { "Sound", SIE::ToFormKey(TESForm::LookupByID<BGSSoundDescriptorForm>(
									 item->soundFormID)) },
						{ "Type", item->type.underlying() } });
			}
			return j;
		};

		j = json{ { "FogDistanceDayNear", weather.fogData.dayNear },
			{ "FogDistanceDayFar", weather.fogData.dayFar },
			{ "FogDistanceDayMax", weather.fogData.dayMax },
			{ "FogDistanceDayPower", weather.fogData.dayPower },
			{ "FogDistanceNightNear", weather.fogData.nightNear },
			{ "FogDistanceNightFar", weather.fogData.nightFar },
			{ "FogDistanceNightMax", weather.fogData.nightMax },
			{ "FogDistanceNightPower", weather.fogData.nightPower },
			{ "DirectionalAmbientLightingColors",
				{ { "Sunrise", weather.directionalAmbientLightingColors[kSunrise] },
					{ "Day", weather.directionalAmbientLightingColors[kDay] },
					{ "Sunset", weather.directionalAmbientLightingColors[kSunset] },
					{ "Night", weather.directionalAmbientLightingColors[kNight] } } },
			{ "SkyUpperColor",
				saveColorData(weather.colorData[TESWeather::ColorTypes::kSkyUpper]) },
			{ "FogNearColor", saveColorData(weather.colorData[TESWeather::ColorTypes::kFogNear]) },
			{ "CloudLayerColor",
				saveColorData(weather.colorData[TESWeather::ColorTypes::kUnknown]) },
			{ "AmbientColor", saveColorData(weather.colorData[TESWeather::ColorTypes::kAmbient]) },
			{ "SunlightColor",
				saveColorData(weather.colorData[TESWeather::ColorTypes::kSunlight]) },
			{ "SunColor", saveColorData(weather.colorData[TESWeather::ColorTypes::kSun]) },
			{ "StarsColor", saveColorData(weather.colorData[TESWeather::ColorTypes::kStars]) },
			{ "SkyLowerColor",
				saveColorData(weather.colorData[TESWeather::ColorTypes::kSkyLower]) },
			{ "HorizonColor", saveColorData(weather.colorData[TESWeather::ColorTypes::kHorizon]) },
			{ "EffectLightingColor",
				saveColorData(weather.colorData[TESWeather::ColorTypes::kEffectLighting]) },
			{ "CloudLodDiffuseColor",
				saveColorData(weather.colorData[TESWeather::ColorTypes::kCloudLODDiffuse]) },
			{ "CloudLodAmbientColor",
				saveColorData(weather.colorData[TESWeather::ColorTypes::kCloudLODAmbient]) },
			{ "FogFarColor", saveColorData(weather.colorData[TESWeather::ColorTypes::kFogFar]) },
			{ "SkyStaticsColor",
				saveColorData(weather.colorData[TESWeather::ColorTypes::kSkyStatics]) },
			{ "WaterMultiplierColor",
				saveColorData(weather.colorData[TESWeather::ColorTypes::kWaterMultiplier]) },
			{ "SunGlareColor",
				saveColorData(weather.colorData[TESWeather::ColorTypes::kSunGlare]) },
			{ "MoonGlareColor",
				saveColorData(weather.colorData[TESWeather::ColorTypes::kMoonGlare]) },
			{ "WindSpeed", weather.data.windSpeed / 255.f },
			{ "TransDelta", weather.data.transDelta * 4.f },
			{ "SunGlare", weather.data.sunGlare / 255.f },
			{ "SunDamage", weather.data.sunDamage / 255.f },
			{ "WindDirection", weather.data.windDirection / 360.f },
			{ "WindDirectionRange", weather.data.windDirectionRange / 180.f },
			{ "PrecipitationBeginFadeIn", weather.data.precipitationBeginFadeIn / 255.f },
			{ "PrecipitationEndFadeOut", weather.data.precipitationEndFadeOut / 255.f },
			{ "ThunderLightningBeginFadeIn", weather.data.thunderLightningBeginFadeIn / 255.f },
			{ "ThunderLightningEndFadeOut", weather.data.thunderLightningEndFadeOut / 255.f },
			{ "ThunderLightningFrequency", weather.data.thunderLightningFrequency / 255.f },
			{ "LightningColor",
				std::format("255, {}, {}, {}", SIE::FloatToU8Color(weather.data.lightningColor.red),
					SIE::FloatToU8Color(weather.data.lightningColor.green),
					SIE::FloatToU8Color(weather.data.lightningColor.blue)) },
			{ "Flags", weather.data.flags.underlying() },
			{ "VisualEffectBegin", weather.data.visualEffectBegin },
			{ "VisualEffectEnd", weather.data.visualEffectEnd },
			{ "Precipitation", SIE::ToFormKey(weather.precipitationData) },
		    { "ImageSpaces", saveImageSpaces(weather.imageSpaces) },
			{ "CloudTextures", saveCloudTextures(weather.cloudTextures) },
			{ "Clouds", saveClouds() }, { "VisualEffect", SIE::ToFormKey(weather.referenceEffect) },
			{ "Aurora", { { "File", weather.aurora.model.c_str() } } },
			{ "SkyStatics", saveSkyStatics() }, { "Sounds" , saveSounds()} };
	}

	void to_json(json& j, const BGSVolumetricLighting& lighting)
	{
		j = json{ { "Intensity", lighting.intensity }, { "CustomColorContribution", lighting.customColor.contribution },
			{ "ColorR", lighting.red }, { "ColorG", lighting.green }, { "ColorB", lighting.blue },
			{ "DensityContribution", lighting.density.contribution },
			{ "DensitySize", lighting.density.size },
			{ "DensityWindSpeed", lighting.density.windSpeed },
			{ "DensityFallingSpeed", lighting.density.fallingSpeed },
			{ "PhaseFunctionContribution", lighting.phaseFunction.contribution },
			{ "PhaseFunctionScattering", lighting.phaseFunction.scattering },
			{ "SamplingRepartitionRangeFactor", lighting.samplingRepartition.rangeFactor } };
	}

	void to_json(json& j, const BGSShaderParticleGeometryData& particle)
	{
		using enum BGSShaderParticleGeometryData::DataID;

		j = json{ { "GravityVelocity", particle.data[static_cast<size_t>(kGravityVelocity)].f },
			{ "RotationVelocity", particle.data[static_cast<size_t>(kGravityVelocity)].f },
			{ "ParticleSizeX", particle.data[static_cast<size_t>(kParticleSizeX)].f },
			{ "ParticleSizeY", particle.data[static_cast<size_t>(kParticleSizeY)].f },
			{ "CenterOffsetMin", particle.data[static_cast<size_t>(kCenterOffsetMin)].f },
			{ "CenterOffsetMax", particle.data[static_cast<size_t>(kCenterOffsetMax)].f },
			{ "InitialRotationRange", particle.data[static_cast<size_t>(kStartRotationRange)].f },
			{ "NumSubtexturesX", particle.data[static_cast<size_t>(kNumSubtexturesX)].i },
			{ "NumSubtexturesY", particle.data[static_cast<size_t>(kNumSubtexturesY)].i },
			{ "Type", particle.data[static_cast<size_t>(kParticleType)].i },
			{ "BoxSize", particle.data[static_cast<size_t>(kBoxSize)].i },
			{ "ParticleDensity", particle.data[static_cast<size_t>(kParticleDensity)].f },
			{ "ParticleTexture", particle.particleTexture.textureName },
		};
	}

	void to_json(json& j, const BGSLightingTemplate& lightingTemplate)
	{
		j = json{
			{ "AmbientColor", lightingTemplate.data.ambient },
			{ "DirectionalColor", lightingTemplate.data.directional},
			{ "FogNearColor", lightingTemplate.data.fogColorNear },
			{ "FogNear", lightingTemplate.data.fogNear },
			{ "FogFar", lightingTemplate.data.fogFar },
			{ "DirectionalRotationXY", lightingTemplate.data.directionalXY },
			{ "DirectionalRotationZ", lightingTemplate.data.directionalZ },
			{ "DirectionalFade", lightingTemplate.data.directionalFade },
			{ "FogClipDistance", lightingTemplate.data.clipDist },
			{ "FogPower", lightingTemplate.data.fogPower },
			{ "FogFarColor", lightingTemplate.data.fogColorFar },
			{ "FogMax", lightingTemplate.data.fogClamp },
			{ "LightFadeStartDistance", lightingTemplate.data.lightFadeStart },
			{ "LightFadeEndDistance", lightingTemplate.data.lightFadeEnd },
			{ "DirectionalAmbientColors", lightingTemplate.directionalAmbientLightingColors },
		};
	}

	void to_json(json& j, const INTERIOR_DATA& data)
	{
		j = json{
			{ "AmbientColor", data.ambient },
			{ "DirectionalColor", data.directional },
			{ "FogNearColor", data.fogColorNear },
			{ "FogNear", data.fogNear },
			{ "FogFar", data.fogFar },
			{ "DirectionalRotationXY", data.directionalXY },
			{ "DirectionalRotationZ", data.directionalZ },
			{ "DirectionalFade", data.directionalFade },
			{ "FogClipDistance", data.clipDist },
			{ "FogPower", data.fogPower },
			{ "FogFarColor", data.fogColorFar },
			{ "FogMax", data.fogClamp },
			{ "LightFadeStartDistance", data.lightFadeStart },
			{ "LightFadeEndDistance", data.lightFadeEnd },
			{ "AmbientColors", data.directionalAmbientLightingColors },
			{ "Inherits", data.lightingTemplateInheritanceFlags.get() },
		};
	}

	void to_json(json& j, const TESObjectCELL& cell)
	{
		const auto imageSpaceExtra = static_cast<const RE::ExtraCellImageSpace*>(
			cell.extraList.GetByType(ExtraDataType::kCellImageSpace));
		const auto skyRegionExtra = static_cast<const RE::ExtraCellSkyRegion*>(
			cell.extraList.GetByType(ExtraDataType::kCellSkyRegion));
		const auto waterExtra = static_cast<const RE::ExtraCellWaterType*>(
			cell.extraList.GetByType(ExtraDataType::kCellWaterType));
		const auto waterEnvMapExtra = static_cast<const RE::ExtraCellWaterEnvMap*>(
			cell.extraList.GetByType(ExtraDataType::kCellWaterEnvMap));

		j = json{
			{ "Flags", cell.cellFlags.get() },
			{ "ImageSpace",
				SIE::ToFormKey(imageSpaceExtra ? imageSpaceExtra->imageSpace : nullptr) },
			{ "LightingTemplate", SIE::ToFormKey(cell.lightingTemplate) },
			{ "SkyAndWeatherFromRegion",
				SIE::ToFormKey(skyRegionExtra ? skyRegionExtra->skyRegion : nullptr) },
			{ "Water", SIE::ToFormKey(waterExtra ? waterExtra->water : nullptr) },
		};

		if (cell.IsInteriorCell())
		{
			j.push_back({ { "Lighting", *cell.cellData.interior } });
			if (waterEnvMapExtra != nullptr)
			{
				j.push_back({ { "WaterEnvironmentMap", waterEnvMapExtra->waterEnvMap.textureName } });
			}
		}
	}

	void to_json(json& j, const NiPoint3& point)
	{
		j = json{
			{ "X", point.x },
			{ "Y", point.y },
			{ "Z", point.z },
		};
	}

	void to_json(json& j, const TESWaterForm& water)
	{
		j = json{
			{ "AngularVelocity", water.angularVelocity },
			{ "DamagePerSecond", water.attackDamage },
			{ "DeepColor", water.data.deepWaterColor },
			{ "DepthNormals", water.data.depthProperties.normals },
			{ "DepthReflections", water.data.depthProperties.reflections },
			{ "DepthRefraction", water.data.depthProperties.refraction },
			{ "DepthSpecularLighting", water.data.depthProperties.specularLighting },
			{ "DisplacementDampner", water.data.displacementDampener },
			{ "DisplacementFalloff", water.data.displacementFalloff },
			{ "DisplacementFoce", water.data.displacementForce },
			{ "DisplacementStartingSize", water.data.displacementSize },
			{ "DisplacementVelocity", water.data.displacementVelocity },
			{ "Flags", water.flags.get() },
			{ "FlowNormalsNoiseTexture", water.noiseTextures[3].textureName },
			{ "FogAboveWaterAmount", water.data.aboveWaterFogAmount },
			{ "FogAboveWaterDistanceFarPlane", water.data.aboveWaterFogDistFar },
			{ "FogAboveWaterDistanceNearPlane", water.data.aboveWaterFogDistNear },
			{ "FogUnderWaterAmount", water.data.underwaterFogAmount },
			{ "FogUnderWaterDistanceFarPlane", water.data.underwaterFogDistFar },
			{ "FogUnderWaterDistanceNearPlane", water.data.underwaterFogDistNear },
			{ "ImageSpace", SIE::ToFormKey(water.imageSpace) },
			{ "LinearVelocity", water.linearVelocity },
			{ "Material", SIE::ToFormKey(water.materialType) },
			{ "NoiseFalloff", water.data.noiseFalloff },
			{ "NoiseLayerOneAmplitudeScale", water.data.amplitudeA[0] },
			{ "NoiseLayerOneTexture", water.noiseTextures[0].textureName },
			{ "NoiseLayerOneUvScale", water.data.uvScaleA[0] },
			{ "NoiseLayerOneWindDirection", water.data.noiseWindDirectionA[0] },
			{ "NoiseLayerOneWindSpeed", water.data.noiseWindSpeedA[0] },
			{ "NoiseLayerThreeAmplitudeScale", water.data.amplitudeA[2] },
			{ "NoiseLayerThreeTexture", water.noiseTextures[2].textureName },
			{ "NoiseLayerThreeUvScale", water.data.uvScaleA[2] },
			{ "NoiseLayerThreeWindDirection", water.data.noiseWindDirectionA[2] },
			{ "NoiseLayerThreeWindSpeed", water.data.noiseWindSpeedA[2] },
			{ "NoiseLayerTwoAmplitudeScale", water.data.amplitudeA[1] },
			{ "NoiseLayerTwoTexture", water.noiseTextures[1].textureName },
			{ "NoiseLayerTwoUvScale", water.data.uvScaleA[1] },
			{ "NoiseLayerTwoWindDirection", water.data.noiseWindDirectionA[1] },
			{ "NoiseLayerTwoWindSpeed", water.data.noiseWindSpeedA[1] },
			{ "Opacity", water.alpha },
			{ "OpenSound", SIE::ToFormKey(water.waterSound) },
			{ "ReflectionColor", water.data.reflectionWaterColor },
			{ "ShallowColor", water.data.shallowWaterColor },
			{ "SpecularBrightness", water.data.specularBrightness },
			{ "SpecularPower", water.data.specularPower },
			{ "SpecularRadius", water.data.specularRadius },
			{ "SpecularSunPower", water.data.sunSpecularPower },
			{ "SpecularSunSparkleMagnitude", water.data.sunSparkleMagnitude },
			{ "SpecularSunSparklePower", water.data.sunSparklePower },
			{ "SpecularSunSpecularMagnitude", water.data.sunSpecularMagnitude },
			{ "Spell", SIE::ToFormKey(water.contactSpell) },
			{ "WaterFresnel", water.data.fresnelAmount },
			{ "WaterReflectionMagnitude", water.data.reflectionMagnitude },
			{ "WaterReflectivity", water.data.reflectionAmount },
			{ "WaterRefractionMagnitude", water.data.refractionMagnitude },
		};
	}

	void to_json(json& j, const TESForm& form)
	{
		switch (form.GetFormType())
		{
		case FormType::Cell:
			to_json(j, static_cast<const TESObjectCELL&>(form));
			break;
		case FormType::ImageSpace:
			to_json(j, static_cast<const TESImageSpace&>(form));
			break;
		case FormType::LightingMaster:
			to_json(j, static_cast<const BGSLightingTemplate&>(form));
			break;
		case FormType::ShaderParticleGeometryData:
			to_json(j, static_cast<const BGSShaderParticleGeometryData&>(form));
			break;
		case FormType::VolumetricLighting:
			to_json(j, static_cast<const BGSVolumetricLighting&>(form));
			break;
		case FormType::Water:
			to_json(j, static_cast<const TESWaterForm&>(form));
			break;
		case FormType::Weather:
			to_json(j, static_cast<const TESWeather&>(form));
			break;
		}
	}
}
