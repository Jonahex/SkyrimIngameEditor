#include "Gui/CellEditor.h"

#include "Gui/ImageSpaceEditor.h"
#include "Gui/Utils.h"
#include "Gui/WaterEditor.h"
#include "Utils/Engine.h"

#include <RE/B/BSTriShape.h>
#include <RE/B/BSWaterShaderMaterial.h>
#include <RE/B/BSWaterShaderProperty.h>
#include <RE/E/ExtraCellImageSpace.h>
#include <RE/E/ExtraCellWaterType.h>
#include <RE/N/NiAVObject.h>
#include <RE/N/NiRTTI.h>
#include <RE/N/NiSourceTexture.h>
#include <RE/T/TESImageSpace.h>
#include <RE/T/TESObjectACTI.h>
#include <RE/T/TESObjectCELL.h>
#include <RE/T/TESWaterDisplacement.h>
#include <RE/T/TESWaterForm.h>
#include <RE/T/TESWaterNormals.h>
#include <RE/T/TESWaterObject.h>
#include <RE/T/TESWaterReflections.h>
#include <RE/T/TESWaterSystem.h>

#include <imgui.h>

namespace SIE
{
	namespace SCellEditor
	{
		void ProcessGeometry(RE::NiAVObject* geometry, RE::TESWaterForm* waterType)
		{
			if (geometry != nullptr)
			{
				/*logger::info("{} {}", static_cast<void*>(geometry), geometry->GetRTTI()->GetName());*/
				if (auto triShape = geometry->AsGeometry())
				{
					auto property = triShape->properties[1].get();
					/*logger::info("{} {}", static_cast<void*>(property),
						property->GetRTTI()->GetName());*/
					auto waterProperty = static_cast<RE::BSWaterShaderProperty*>(property);
					auto waterMaterial =
						static_cast<RE::BSWaterShaderMaterial*>(waterProperty->material);
					waterType->FillMaterial(*waterMaterial);
					//waterProperty->alpha = waterMaterial->alpha;

					/*logger::info("ProcessGeometry: {} {}", static_cast<void*>(triShape),
						static_cast<void*>(waterMaterial));*/

					/*const REL::Relocation<void (*)(RE::BSWaterShaderProperty*,
						RE::BSWaterShaderMaterial*, bool)>
						func{ REL::ID(
						105544) };
					func(waterProperty, waterMaterial, false);*/
				}
				else if (auto node = geometry->AsNode())
				{
					for (const auto& child : node->children)
					{
						ProcessGeometry(child.get(), waterType);
					}
				}
			}
		}
	}

    void CellEditor(const char* label, RE::TESObjectCELL& cell)
	{
		ImGui::PushID(label);

		if (const auto extra = static_cast<RE::ExtraCellImageSpace*>(
				cell.extraList.GetByType(RE::ExtraDataType::kCellImageSpace)))
		{
			if (PushingCollapsingHeader("Imagespace"))
			{
				FormEditor(&cell, FormSelector<true>("Imagespace", extra->imageSpace));
				ImageSpaceEditor("ImageSpaceEditor", *extra->imageSpace);
				ImGui::TreePop();
			}
		}

		if (cell.cellFlags.any(RE::TESObjectCELL::Flag::kHasWater))
		{
			if (const auto extra = static_cast<RE::ExtraCellWaterType*>(cell.extraList.GetByType(RE::ExtraDataType::kCellWaterType)))
			{
				if (extra->water != nullptr)
				{
					if (PushingCollapsingHeader("Water"))
					{
						FormEditor(&cell, FormSelector<false>("Water Type", extra->water));
						WaterEditor("WaterEditor", *extra->water);
						ImGui::TreePop();

						const REL::Relocation<bool (*)(RE::TESObjectREFR*)> loadFunc{ RELOCATION_ID(
							19409, 19837) };
						const REL::Relocation<bool (*)(RE::TESObjectREFR*)> unloadFunc{
							RELOCATION_ID(19410, 19838)
						};
						const REL::Relocation<void (*)(RE::TESWaterNormals*)> normalsUpdateFunc{
							REL::ID(32170)
						};
						const REL::Relocation<void (*)(RE::TESWaterReflections*)> reflectionsUpdateFunc{
							REL::ID(32160)
						};

						auto waterSystem = RE::TESWaterSystem::GetSingleton();

						logger::info("{}", static_cast<void*>(extra->water->waterShaderMaterial));

						for (const auto& item : cell.references) 
						{
							if (item->IsWater())
							{
								auto activator = static_cast<RE::TESObjectACTI*>(item->GetBaseObject());
								activator->waterForm = extra->water;

								/*unloadFunc(item.get());
								loadFunc(item.get());*/

								auto geometry = item->Get3D2();
								SCellEditor::ProcessGeometry(geometry,
									item->GetBaseObject()->GetWaterType());

								/*RE::NiUpdateData updateData{ 0.f, RE::NiUpdateData::Flag::kNone };
								geometry->Update(updateData);*/
							}
						}

						for (const auto& item : waterSystem->waterObjects)
						{
							if (item->waterType == extra->water)
							{
								//logger::info("Water object: {}", static_cast<void*>(item.get()));
								/*SCellEditor::ProcessGeometry(item->shape.get(), extra->water);*/
								if (item->normals != nullptr)
								{
									//logger::info("Normals: {}", static_cast<void*>(item->normals->waterMaterial));
									//item->normals->waterType = extra->water;
									normalsUpdateFunc(item->normals.get());
								}
								if (item->normals != nullptr)
								{
									/*logger::info("Reflections: {}",
										static_cast<void*>(item->reflections->waterMaterial));*/
								}
								if (item->displacement != nullptr)
								{
									/*logger::info("Displacement: {}",
										static_cast<void*>(
											item->displacement->displacementGeometry.get()));*/
									/*SCellEditor::ProcessGeometry(
										item->displacement->displacementGeometry.get(),
										extra->water);*/
								}
								if (item->waterRippleObject != nullptr)
								{
									/*logger::info("Ripple: {}",
										static_cast<void*>(
											item->waterRippleObject.get()));*/
									/*SCellEditor::ProcessGeometry(item->waterRippleObject.get(),
										extra->water);*/
								}
							}
						}

						/*for (auto it = cell.references.begin(); it != cell.references.end(); ++it)
						{
							if ((*it)->IsWater())
							{
								unloadFunc(it->get());
								(*it)->DeleteThis();
								it = cell.references.erase(it);
							}
						}

						using func_t = void (*)(void*, RE::TESObjectCELL*, bool);
						const REL::Relocation<func_t> func{ RELOCATION_ID(31230, 32030) };
						const REL::Relocation<void**> singleton{ RELOCATION_ID(514289, 400449) };
						func(*singleton.get(), &cell, false);*/
					}
				}
			}
		}

		ImGui::PopID();
    }
}
