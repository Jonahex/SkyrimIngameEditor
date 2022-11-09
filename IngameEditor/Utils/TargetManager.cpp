#include "Utils/TargetManager.h"

#include "Utils/GraphTracker.h"

#include <RE/B/BSGeometry.h>
#include <RE/B/BSEffectShaderMaterial.h>
#include <RE/B/BSEffectShaderProperty.h>
#include <RE/B/BSLightingShaderMaterialBase.h>
#include <RE/B/BSLightingShaderProperty.h>
#include <RE/B/BSWaterShaderMaterial.h>
#include <RE/B/BSWaterShaderProperty.h>
#include <RE/T/TESObjectREFR.h>

namespace SIE
{
	namespace STargetManager
	{
		struct HighlightBackup
		{
			RE::NiColor emissiveColor;
			float emissiveMultiple;
		};

		void SetHighlight(RE::NiAVObject* object, bool isEnable)
		{
			static std::unordered_map<RE::NiPointer<RE::NiProperty>, HighlightBackup> PropertyBackups;
			static std::unordered_map<RE::BSTSmartPointer<RE::BSShaderMaterial>,
				RE::BSShaderMaterial*>
				MaterialBackups;

			if (object == nullptr)
			{
				return;
			}
			if (auto node = object->AsNode())
			{
				for (const auto& child : node->children)
				{
					SetHighlight(child.get(), isEnable);
				}
			}
			else if (auto geometry = object->AsGeometry())
			{
				const auto property = geometry->properties[RE::BSGeometry::States::kEffect];
				if (property != nullptr && property->GetType() == RE::NiProperty::Type::kShade)
				{
					const auto material =
						static_cast<RE::BSShaderProperty*>(property.get())->material;
					if (material->GetType() == RE::BSShaderMaterial::Type::kLighting)
					{
						const auto lightingProperty =
							static_cast<RE::BSLightingShaderProperty*>(property.get());
						if (isEnable)
						{
							PropertyBackups[property] = { *lightingProperty->emissiveColor,
								lightingProperty->emissiveMult };
							*lightingProperty->emissiveColor = { 1.f, 0.f, 0.f };
							lightingProperty->emissiveMult = 10.f;
						}
						else if (auto it = PropertyBackups.find(property);
								 it != PropertyBackups.cend())
						{
							*lightingProperty->emissiveColor = it->second.emissiveColor;
							lightingProperty->emissiveMult = it->second.emissiveMultiple;

							PropertyBackups.erase(it);
						}
					}
					else if (material->GetType() == RE::BSShaderMaterial::Type::kEffect)
					{
						const auto effectProperty =
							static_cast<RE::BSEffectShaderProperty*>(property.get());
						const auto effectMaterial =
							static_cast<RE::BSEffectShaderMaterial*>(effectProperty->material);
						if (isEnable)
						{
							const auto newMaterial = RE::BSTSmartPointer(
								static_cast<RE::BSEffectShaderMaterial*>(effectMaterial->Create()));
							newMaterial->CopyMembers(effectMaterial);
							newMaterial->baseColor = { 1.f, 0.f, 0.f, 1.f };
							newMaterial->baseColorScale = 10.f;
							effectProperty->material = newMaterial.get();

							MaterialBackups[newMaterial] = effectMaterial;
						}
						else
						{
							if (auto it = MaterialBackups.find(RE::BSTSmartPointer(effectMaterial));
								it != MaterialBackups.cend())
							{
								effectProperty->material = it->second;
								MaterialBackups.erase(it);
							}
						}
					}
					else if (material->GetType() == RE::BSShaderMaterial::Type::kWater)
					{
						const auto waterProperty =
							static_cast<RE::BSWaterShaderProperty*>(property.get());
						const auto waterMaterial =
							static_cast<RE::BSWaterShaderMaterial*>(waterProperty->material);
						if (isEnable)
						{
							const auto newMaterial = RE::BSTSmartPointer(
								static_cast<RE::BSWaterShaderMaterial*>(waterMaterial->Create()));
							newMaterial->CopyMembers(waterMaterial);
							newMaterial->shallowWaterColor = { 1.f, 0.f, 0.f };
							newMaterial->deepWaterColor = { 1.f, 0.f, 0.f };
							newMaterial->reflectionColor = { 1.f, 0.f, 0.f };
							waterProperty->material = newMaterial.get();

							MaterialBackups[newMaterial] = waterMaterial;
						}
						else
						{
							if (auto it = MaterialBackups.find(RE::BSTSmartPointer(waterMaterial));
								it != MaterialBackups.cend())
							{
								waterProperty->material = it->second;
								MaterialBackups.erase(it);
							}
						}
					}
				}
			}
		}
	}

	TargetManager& TargetManager::Instance()
	{ 
		static TargetManager instance;
		return instance;
	}

	RE::TESObjectREFR* TargetManager::GetTarget() const 
	{ 
		return target.get(); 
	}

	void TargetManager::TrySetTargetAt(int screenX, int screenY)
	{
		struct ConsoleProxy
		{
			uint8_t pad00[56];
			RE::BSTArray<RE::ObjectRefHandle> array;
			int32_t index = -1;
		} proxy;

		static const REL::Relocation<void (*)(ConsoleProxy*)> consoleSelection(
			RE::Offset::Console::SelectReference);
		static const REL::Relocation<float**> screenPoint(RELOCATION_ID(517043, 403551));

		float backupX;
		float backupY;
		backupX = (*screenPoint)[1];
		backupY = (*screenPoint)[2];

		(*screenPoint)[1] = screenX;
		(*screenPoint)[2] = screenY;
		consoleSelection(&proxy);

		if (proxy.index >= 0 && proxy.index < static_cast<int32_t>(proxy.array.size()))
		{
			auto newTarget = proxy.array[proxy.index].get();
			if (target == newTarget)
			{
				newTarget = nullptr;
			}
			if (target != newTarget)
			{
				if (target != nullptr)
				{
					STargetManager::SetHighlight(target->Get3D2(), false);
				}
				target = newTarget;
				if (target != nullptr && enableTargetHighlight)
				{
					STargetManager::SetHighlight(target->Get3D2(), true);
				}
				GraphTracker::Instance().SetTarget(target.get());
			}
		}

		(*screenPoint)[1] = backupX;
		(*screenPoint)[2] = backupY;
	}

	bool TargetManager::GetEnableTargetHighlight() const 
	{ 
		return enableTargetHighlight;
	}

	void TargetManager::SetEnableTargetHightlight(bool value) 
	{ 
		enableTargetHighlight = value;

		if (target != nullptr)
		{
			STargetManager::SetHighlight(target->Get3D2(), enableTargetHighlight);
		}
	}
}
