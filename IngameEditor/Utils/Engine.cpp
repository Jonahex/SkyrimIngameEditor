#include "Utils/Engine.h"

#include <RE/B/BGSGrassManager.h>
#include <RE/B/bhkWorldObject.h>
#include <RE/B/BSMultiBoundShape.h>
#include <RE/B/BSTriShape.h>
#include <RE/B/BSWaterShaderMaterial.h>
#include <RE/B/BSWaterShaderProperty.h>
#include <RE/G/GridCellArray.h>
#include <RE/N/NiCollisionObject.h>
#include <RE/N/NiExtraData.h>
#include <RE/N/NiObjectNET.h>
#include <RE/N/NiRTTI.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/S/Sky.h>
#include <RE/T/TES.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESGlobal.h>
#include <RE/T/TESObjectCELL.h>
#include <RE/T/TESWaterDisplacement.h>
#include <RE/T/TESWaterForm.h>
#include <RE/T/TESWaterSystem.h>
#include <RE/T/TESWorldSpace.h>

#include <magic_enum.hpp>

namespace SIE
{
	float U8ToFloatColor(uint8_t value)
	{
	    return std::min(static_cast<float>(value) / 255.f, 1.f);
	}

	uint8_t FloatToU8Color(float value)
	{
	    return static_cast<uint8_t>(std::min(255.f, 255.f * value));
	}

	std::string GetCellFullName(const RE::TESObjectCELL& cell)
	{
		if (cell.IsInteriorCell())
		{
			return GetFullName(cell);
		}
		return std::format("{} [{:X}] <{}:({},{})>", cell.GetFormEditorID(), cell.GetFormID(), cell.worldSpace->GetFormEditorID(), cell.cellData.exterior->cellX, cell.cellData.exterior->cellY);
	}

	std::string GetFullName(const RE::TESForm& form)
	{
		return std::format("{} [{:X}]", form.GetFormEditorID(), form.GetFormID());
	}

	std::string GetTypedName(const RE::TESForm& form)
	{
		return std::format("<{}> {} [{:X}]", magic_enum::enum_name(form.formType.get()), form.GetFormEditorID(), form.GetFormID());
	}

	RE::TESGlobal* FindGlobal(const std::string& editorId)
	{
		const auto dataHandler = RE::TESDataHandler::GetSingleton();
		const auto& globals = dataHandler->GetFormArray<RE::TESGlobal>();
		for (const auto& global : globals)
		{
			if (global->GetFormEditorID() == editorId)
			{
				return global;
			}
		}
		return nullptr;
	}

	void ResetTimeTo(float time)
	{
		const auto gameHour = FindGlobal("GameHour");
		gameHour->value = time;
	}

	void ResetTimeForFog(RE::Sky& sky, bool isDay) 
	{
		float timeToSet = 0.f;

		if (isDay)
		{
			const float sunriseEndTime = sky.GetSunriseEndTime();
			float sunsetBeginTime = sky.GetSunsetBeginTime();

			if (sunsetBeginTime < sunriseEndTime)
			{
				sunsetBeginTime += 24.f;
			}
			timeToSet = (sunriseEndTime + sunsetBeginTime) / 2.f;
		}
		else
		{
			const float dayBeginTime = sky.GetDayBeginTime();
			float nightBeginTime = sky.GetNightBeginTime();

			if (nightBeginTime < dayBeginTime)
			{
				nightBeginTime += 24.f;
			}
			timeToSet = (nightBeginTime + dayBeginTime);
		}

		ResetTimeTo(timeToSet);
	}

	void ResetTimeForColor(RE::Sky& sky, RE::TESWeather::ColorTimes::ColorTime colorTime)
	{
		float timeToSet = 0.f;

		float sunriseEndTime = sky.GetSunriseEndTime();
		float sunsetBeginTime = sky.GetSunsetBeginTime();
		float dayBeginTime = sky.GetDayBeginTime();
		float nightBeginTime = sky.GetNightBeginTime();

		switch (colorTime)
		{
		case RE::TESWeather::ColorTime::kSunrise:
			{
				if (sunriseEndTime < dayBeginTime)
				{
					sunriseEndTime += 24.f;
				}
				timeToSet = (sunriseEndTime + dayBeginTime) / 2.f;
			}
			break;
		case RE::TESWeather::ColorTime::kDay:
			{
				if (sunsetBeginTime < sunriseEndTime)
				{
					sunsetBeginTime += 24.f;
				}
				timeToSet = (sunsetBeginTime + sunriseEndTime) / 2.f;
			}
			break;
		case RE::TESWeather::ColorTime::kSunset:
			{
				if (nightBeginTime < sunsetBeginTime)
				{
					nightBeginTime += 24.f;
				}
				timeToSet = (nightBeginTime + sunsetBeginTime) / 2.f;
			}
			break;
		case RE::TESWeather::ColorTime::kNight:
			{
				if (dayBeginTime < nightBeginTime)
				{
					nightBeginTime += 24.f;
				}
				timeToSet = (dayBeginTime + nightBeginTime) / 2.f;
			}
			break;
		default:;
		}

		ResetTimeTo(timeToSet);
	}

	void SetVisibility(RE::VISIBILITY visibility, bool isVisible)
	{
		RE::TES::GetSingleton()->SetVisibility(visibility, isVisible, false);
		if (visibility == RE::VISIBILITY::kWater)
		{
			RE::TESWaterSystem::GetSingleton()->SetVisibility(isVisible, false, false);
		}
	}

	void UpdateWaterGeometry(RE::NiAVObject* geometry, RE::TESWaterForm* waterType)
	{
		if (geometry != nullptr)
		{
			if (auto triShape = geometry->AsGeometry())
			{
				auto property = triShape->properties[1].get();
				if (std::strcmp(property->GetRTTI()->name, "BSWaterShaderProperty") == 0)
				{
					auto waterProperty = static_cast<RE::BSWaterShaderProperty*>(property);
					auto waterMaterial =
						static_cast<RE::BSWaterShaderMaterial*>(waterProperty->material);
					waterType->FillMaterial(*waterMaterial);
				}
			}
			else if (auto node = geometry->AsNode())
			{
				for (const auto& child : node->children)
				{
					UpdateWaterGeometry(child.get(), waterType);
				}
			}
		}
	}

	struct UnkSingleton
	{
		uint8_t unk00[0x2028];
		RE::BSTArray<RE::BSMultiBoundShape*> multiBounds;
		RE::NiNode* defaultProceduralWater;
		RE::NiNode* defaultWaterRipples;
	};

	void LoadCellAutoWater(UnkSingleton* unkSingleton, RE::TESObjectCELL* cell, bool a3)
	{
		static const REL::Relocation<RE::NiNode*(RE::NiNode*)> cloneFunc{ RELOCATION_ID(
			68835, 70187) };
		static const REL::Relocation<int32_t*> uGridsToLoadPtr{ RELOCATION_ID(501245, 359676) };
		static const REL::Relocation<void(UnkSingleton*, RE::TESObjectCELL*, RE::BSGeometry*)>
			setupAutoWaterGeometryFunc{ RELOCATION_ID(31231, 32031) };
		static const REL::Relocation<void(UnkSingleton*, RE::TESObjectCELL*)> setupCellMultiboundsFunc{
			REL::ID(32035)
		};
		static const REL::Relocation<void(UnkSingleton*, RE::TESObjectCELL*, RE::NiNode*)> createAutoWaterObjectFunc{
			RELOCATION_ID(31236, 32036) };
		static const REL::Relocation<void(RE::TESObjectCELL*, RE::NiNode*)> setAutoWaterFunc{
			RELOCATION_ID(18559, 19020)
		};

		if (cell == nullptr || cell->cellFlags.none(RE::TESObjectCELL::Flag::kHasWater))
		{
			return;
		}

		auto tes = RE::TES::GetSingleton();
		auto waterSystem = RE::TESWaterSystem::GetSingleton();
		const auto uGridsToLoad = *uGridsToLoadPtr;

		auto waterNode = cloneFunc(unkSingleton->defaultProceduralWater);
		auto waterShape = waterNode->children[0]->AsGeometry();
		auto waterProperty = static_cast<RE::BSWaterShaderProperty*>(waterShape->properties[1].get());
		waterProperty->cellX = cell->GetCoordinates()->cellX;
		waterProperty->cellY = cell->GetCoordinates()->cellY;
		waterProperty->unk98 = (uGridsToLoad + waterProperty->cellX + uGridsToLoad / 2 -
								   tes->unk0B0 + waterSystem->unk018) %
		                       uGridsToLoad;
		waterProperty->unk9C = (uGridsToLoad + waterProperty->cellY + uGridsToLoad / 2 -
								   tes->unk0B4 + waterSystem->unk01C) %
		                       uGridsToLoad;

		if (!a3)
		{
			setupAutoWaterGeometryFunc(unkSingleton, cell, waterShape);
		}
		//setupCellMultiboundsFunc(unkSingleton, cell);
		createAutoWaterObjectFunc(unkSingleton, cell, waterNode);
		setAutoWaterFunc(cell, waterNode);
		//unkSingleton->multiBounds.clear();
	}

	void ReloadWaterObjects()
	{
		auto tes = RE::TES::GetSingleton();
		auto waterSystem = RE::TESWaterSystem::GetSingleton();
		const auto player = RE::PlayerCharacter::GetSingleton();

		static const REL::Relocation<void(RE::TESObjectREFR*)> loadFunc{ RELOCATION_ID(
			19409, 19837) };
		static const REL::Relocation<void(RE::TESObjectREFR*)> unloadFunc{ RELOCATION_ID(
			19410, 19838) };
		static const REL::Relocation<void(RE::TESObjectREFR*)> createCollisionFunc{ RELOCATION_ID(
			19309, 19736) };

		static const REL::Relocation<UnkSingleton**> unkWaterSingleton{ RELOCATION_ID(514289,
			400449) };
		static const REL::Relocation<void(void*, RE::TESObjectCELL*, bool)> loadCellAutoWaterFunc{
			RELOCATION_ID(31230, 32030)
		};

		tes->ForEachReference(
			[&](RE::TESObjectREFR* ref)
			{
				if (ref->IsWater())
				{
					ref->Disable();
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});

		if (auto cell = player->parentCell; !cell->IsInteriorCell() && cell->cellFlags.any(RE::TESObjectCELL::Flag::kHasWater))
		{
			for (uint32_t x = 0; x < tes->gridCells->length; ++x)
			{
				for (uint32_t y = 0; y < tes->gridCells->length; ++y)
				{
					if (auto cell = tes->gridCells->GetCell(x, y))
					{
						const auto& cell3d = cell->loadedData->cell3D;
						if (cell3d != nullptr && cell3d->children.size() > 2)
						{
							cell3d->children[2]->collisionObject.reset();
						}
						cell->autoWaterObjects.clear();
						cell->placeableWaterObjects.clear();
						cell->waterFalls.clear();
						//cell->extraList.RemoveByType(RE::ExtraDataType::kWaterData);
						loadCellAutoWaterFunc(*unkWaterSingleton, cell, false);
						//LoadCellAutoWater(*unkWaterSingleton, cell, false);
					}
				}
			}
		}

		tes->ForEachReference(
			[&](RE::TESObjectREFR* ref)
			{
				if (ref->IsWater())
				{
					if (ref->formFlags & RE::TESForm::RecordFlags::kTemporary)
					{
						//ref->InitHavok();
						//createCollisionFunc(&ref);
					}
					else
					{
						ref->Enable(false);
					}
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
	}

	bool IsGrassVisible() 
	{ 
		const auto grassManager = RE::BGSGrassManager::GetSingleton();
		return !grassManager->grassNode->flags.any(RE::NiAVObject::Flag::kHidden);
	}

	void SetGrassVisible(bool value)
	{
		static const REL::Relocation<void(RE::TES*)> enableFunc{ RELOCATION_ID(13190, 13335) };
		static const REL::Relocation<void(RE::TES*)> disableFunc{ RELOCATION_ID(13191, 13336) };

		const auto grassManager = RE::BGSGrassManager::GetSingleton();
		auto tes = RE::TES::GetSingleton();
		grassManager->enableGrass = value;
		if (value)
		{
			grassManager->grassNode->flags.reset(RE::NiAVObject::Flag::kHidden);
			enableFunc(tes);
		}
		else
		{
			grassManager->grassNode->flags.set(RE::NiAVObject::Flag::kHidden);
			disableFunc(tes);
		}
	}

	std::string GetFullName(const RE::NiObject& object) 
	{
		if (const auto objectNet = netimmerse_cast<const RE::NiObjectNET*>(&object))
		{
			return std::format("<{}> {}", objectNet->GetRTTI()->name, objectNet->name.c_str());
		}
		else if (const auto extraData = netimmerse_cast<const RE::NiExtraData*>(&object))
		{
			return std::format("<{}> {}", extraData->GetRTTI()->name, extraData->name.c_str());
		}
		return std::format("<{}>", object.GetRTTI()->name);
	}
}
