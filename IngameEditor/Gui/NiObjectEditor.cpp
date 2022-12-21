#include "Gui/NiObjectEditor.h"

#include "Gui/NiTransformEditor.h"
#include "Gui/Utils.h"

#include <RE/B/BSGeometry.h>
#include <RE/B/BSLightingShaderMaterial.h>
#include <RE/B/BSLightingShaderProperty.h>
#include <RE/B/BSXFlags.h>
#include <RE/N/NiCollisionObject.h>
#include <RE/N/NiNode.h>
#include <RE/N/NiRTTI.h>
#include <RE/N/NiSkinInstance.h>
#include <RE/N/NiTimeController.h>

#include <imgui.h>

namespace SIE
{
	namespace SNiObjectEditor
	{
		using NiObjectEditorFunctor = bool(*)(RE::NiObject&);
		using BSShaderMaterialEditorFunctor = bool(*)(RE::BSShaderMaterial&);

		static bool NiObjectEditor(RE::NiObject& object)
		{ 
			return false; 
		}

		static bool BSXFlagsEditor(RE::NiObject& object) 
		{
			auto& bsxFlags = static_cast<RE::BSXFlags&>(object);

			bool wasEdited = false;

			for (const auto& [value, name] : magic_enum::enum_entries<RE::BSXFlags::Flag>())
			{
				if (value == RE::BSXFlags::Flag::kNone)
				{
					continue;
				}
				const auto intValue = static_cast<uint32_t>(value);
				if (ImGui::CheckboxFlags(name.data(), &bsxFlags.value, intValue))
				{
					wasEdited = true;
				}
			}

			return wasEdited;
		}

		static bool NiObjectNETEditor(RE::NiObject& object) 
		{
			auto& objectNet = static_cast<RE::NiObjectNET&>(object);

			bool wasEdited = false;

			if (objectNet.extraDataSize > 0)
			{
				if (PushingCollapsingHeader("Extra Data"))
				{
					for (uint16_t extraIndex = 0; extraIndex < objectNet.extraDataSize; ++extraIndex)
					{
						if (DispatchableNiObjectEditor(std::to_string(extraIndex).c_str(),
							*objectNet.extra[extraIndex]))
						{
							wasEdited = true;
						}
					}
					ImGui::TreePop();
				}
			}

			if (objectNet.controllers != nullptr)
			{
				if (PushingCollapsingHeader("Controllers"))
				{
					size_t index = 0;
					auto controller = objectNet.controllers.get();
					do {
						if (DispatchableNiObjectEditor(std::to_string(index++).c_str(),
								*controller))
						{
							wasEdited = true;
						}
						controller = controller->next.get();
					} while (controller != nullptr);
					ImGui::TreePop();
				}
			}

			return wasEdited;
		}

		static bool NiAVObjectEditor(RE::NiObject& object) 
		{
			auto& avObject = static_cast<RE::NiAVObject&>(object);

			bool wasEdited = false;

			if (NiObjectNETEditor(avObject))
			{
				wasEdited = true;
			}

			if (avObject.collisionObject != nullptr)
			{
				if (PushingCollapsingHeader("Collision"))
				{
					if (DispatchableNiObjectEditor("", *avObject.collisionObject))
					{
						wasEdited = true;
					}
					ImGui::TreePop();
				}
			}

			RE::NiTransform parentTransform;
			if (RE::NiNode* parent = avObject.parent)
			{
				parentTransform = parent->world;
			}
			if (NiTransformEditor("Transform", avObject.local, parentTransform))
			{
				wasEdited = true;
			}

			if (PushingCollapsingHeader("Flags"))
			{
				for (const auto& [value, name] : magic_enum::enum_entries<RE::NiAVObject::Flag>())
				{
					if (value == RE::NiAVObject::Flag::kNone)
					{
						continue;
					}
					if (FlagEdit(name.data(), avObject.flags, value))
					{
						wasEdited = true;
					}
				}
				ImGui::TreePop();
			}

			if (ImGui::SliderFloat("Fade Amount", &avObject.fadeAmount, 0.f, 1.f))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiNodeEditor(RE::NiObject& object) 
		{
			auto& node = static_cast<RE::NiNode&>(object);

			bool wasEdited = false;

			if (NiAVObjectEditor(node))
			{
				wasEdited = true;
			}

			if (!node.children.empty())
			{
				if (PushingCollapsingHeader("Children"))
				{
					size_t index = 0;
					for (auto& child : node.children)
					{
						if (child != nullptr)
						{
							if (DispatchableNiObjectEditor(std::to_string(index++).c_str(), *child))
							{
								wasEdited = true;
							}
						}
					}
					ImGui::TreePop();
				}
			}

			return wasEdited;
		}

		static bool BSGeometryEditor(RE::NiObject& object)
		{
			auto& geometry = static_cast<RE::BSGeometry&>(object);

			bool wasEdited = false;

			if (NiAVObjectEditor(geometry))
			{
				wasEdited = true;
			}

			if (PushingCollapsingHeader("Properties"))
			{
				size_t index = 0;
				for (const auto& property : geometry.properties)
				{
					if (property != nullptr)
					{
						if (DispatchableNiObjectEditor(std::to_string(index++).c_str(), *property))
						{
							wasEdited = true;
						}
					}
				}
				ImGui::TreePop();
			}

			if (geometry.skinInstance != nullptr)
			{
				if (PushingCollapsingHeader("Skin"))
				{
					if (DispatchableNiObjectEditor("", *geometry.skinInstance))
					{
						wasEdited = true;
					}
					ImGui::TreePop();
				}
			}

			return wasEdited;
		}

		/*static bool BSShaderMaterialEditor(RE::BSShaderMaterial& material) 
		{

		}

		static NiObjectEditorFunctor DispatchBSShaderMaterialEditor(const void* object)
		{
			static std::unordered_map<const TypeDescriptor*, BSShaderMaterialEditorFunctor> editors{
				{ &*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSShaderMaterial),
					&BSShaderMaterialEditor },
			};

			const TypeDescriptor* currentType = GetTypeDescriptor(object);
			currentType->pVFTable->
			auto it = editors.find(currentRtti);
			while (it == editors.cend())
			{
				currentRtti = currentRtti->baseRTTI;
				if (currentRtti == nullptr)
				{
					return nullptr;
				}
				it = editors.find(currentRtti);
			}
			return it->second;
		}

		static bool DispatchableBSShaderMaterialEditor(const char* label,
			RE::BSShaderMaterial& material) 
		{
			ImGui::PushID(label);

			bool wasEdited = false;

			std::string name = "<BSShaderMaterial>";
			BSShaderMaterialEditorFunctor editor = &BSShaderMaterialEditor;
			switch (material.GetType())
			{
				case RE::BSShaderMaterial::Type::kLighting:
				{
					name = "<BSLightingShaderMaterialBase>";
					switch (material.GetFeature())
					{
						case RE::BSShaderMaterial::Feature::kDefault:
						{
							name = "<BSLightingShaderMaterial>";
							break;
						}
						case RE::BSShaderMaterial::Feature::kEnvironmentMap:
						{
							name = "<BSLightingShaderMaterialEnvmap>";
							break;
						}
						case RE::BSShaderMaterial::Feature::kGlowMap:
						{
							name = "<BSLightingShaderMaterialGlowmap>";
							break;
						}
						case RE::BSShaderMaterial::Feature::kParallax:
						{
							name = "<BSLightingShaderMaterialParallax>";
							break;
						}
						case RE::BSShaderMaterial::Feature::kFaceGen:
						{
							name = "<BSLightingShaderMaterialFacegen>";
							break;
						}
						case RE::BSShaderMaterial::Feature::kFaceGenRGBTint:
						{
							name = "<BSLightingShaderMaterialFacegenTint>";
							break;
						}
						case RE::BSShaderMaterial::Feature::kHairTint:
						{
							name = "<BSLightingShaderMaterialHairTint>";
							break;
						}
						case RE::BSShaderMaterial::Feature::kParallaxOcc:
						{
							name = "<BSLightingShaderMaterialParallaxOcc>";
							break;
						}
						case RE::BSShaderMaterial::Feature::kLODLand:
						{
							name = "<BSLightingShaderMaterialLODLandscape>";
							break;
						}
						case RE::BSShaderMaterial::Feature::kMultilayerParallax:
						{
							name = "<BSLightingShaderMaterialMultiLayerParallax>";
							break;
						}
						case RE::BSShaderMaterial::Feature::kParallaxOcc:
						{
							name = "<BSLightingShaderMaterialParallaxOcc>";
							break;
						}
						case RE::BSShaderMaterial::Feature::kParallaxOcc:
						{
							name = "<BSLightingShaderMaterialParallaxOcc>";
							break;
						}
						case RE::BSShaderMaterial::Feature::kParallaxOcc:
						{
							name = "<BSLightingShaderMaterialParallaxOcc>";
							break;
						}
					}
					break;
				}
				case RE::BSShaderMaterial::Type::kEffect:
				{
					name = "<BSEffectShaderMaterial>";
					break;
				}
				case RE::BSShaderMaterial::Type::kWater:
				{
					name = "<BSWaterShaderMaterial>";
					break;
				}
			}

			const auto editor = DispatchEditor(*rtti);
			if (editor != nullptr)
			{
				const std::string name = GetFullName(object);
				if (PushingCollapsingHeader(name.c_str()))
				{
					wasEdited = editor(object);
					ImGui::TreePop();
				}
			}

			ImGui::PopID();

			return wasEdited;
		}*/

		static bool BSShaderPropertyEditor(RE::NiObject& object)
		{
			auto& bsShaderProperty = static_cast<RE::BSShaderProperty&>(object);

			bool wasEdited = false;

			if (NiObjectNETEditor(bsShaderProperty))
			{
				wasEdited = true;
			}

			if (ImGui::SliderFloat("Alpha", &bsShaderProperty.alpha, 0.f, 1.f))
			{
				wasEdited = true;
			}

			if (PushingCollapsingHeader("Flags"))
			{
				for (const auto& [value, name] :
					magic_enum::enum_entries<RE::BSShaderProperty::EShaderPropertyFlag>())
				{
					if (FlagEdit(name.data(), bsShaderProperty.flags, value))
					{
						wasEdited = true;
					}
				}
				ImGui::TreePop();
			}

			if (bsShaderProperty.material != nullptr)
			{
				if (PushingCollapsingHeader("Material"))
				{
					ImGui::TreePop();
					/*if (DispatchableNiObjectEditor("",
							*bsShaderProperty.material))
					{
						wasEdited = true;
					}*/
				}
			}

			return wasEdited;
		}

		static bool BSLightingShaderPropertyEditor(RE::NiObject& object)
		{
			auto& bsLightingShaderProperty = static_cast<RE::BSLightingShaderProperty&>(object);

			bool wasEdited = false;

			if (BSShaderPropertyEditor(bsLightingShaderProperty))
			{
				wasEdited = true;
			}

			if (bsLightingShaderProperty.emissiveColor != nullptr)
			{
				if (ImGui::ColorEdit3("Emissive Color",
						&bsLightingShaderProperty.emissiveColor->red))
				{
					wasEdited = true;
				}
			}

			if (ImGui::DragFloat("Emissive Multiplier", &bsLightingShaderProperty.emissiveMult, 0.01f, 0.f))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static NiObjectEditorFunctor DispatchEditor(const RE::NiRTTI& rtti)
		{
			static std::unordered_map<const RE::NiRTTI*, NiObjectEditorFunctor> editors
			{
				{ &*REL::Relocation<RE::NiRTTI*>(RE::NiRTTI_NiObject), &NiObjectEditor },
				{ &*REL::Relocation<RE::NiRTTI*>(RE::NiRTTI_NiObjectNET), &NiObjectNETEditor },
				{ &*REL::Relocation<RE::NiRTTI*>(RE::NiRTTI_NiAVObject), &NiAVObjectEditor },
				{ &*REL::Relocation<RE::NiRTTI*>(RE::NiRTTI_NiNode), &NiNodeEditor },
				{ &*REL::Relocation<RE::NiRTTI*>(RE::NiRTTI_BSXFlags), &BSXFlagsEditor },
				{ &*REL::Relocation<RE::NiRTTI*>(RE::NiRTTI_BSGeometry), &BSGeometryEditor },
				{ &*REL::Relocation<RE::NiRTTI*>(RE::NiRTTI_BSShaderProperty),
					&BSShaderPropertyEditor },
				{ &*REL::Relocation<RE::NiRTTI*>(RE::NiRTTI_BSLightingShaderProperty),
					&BSLightingShaderPropertyEditor },
			};

			const RE::NiRTTI* currentRtti = &rtti;
			auto it = editors.find(currentRtti);
			while (it == editors.cend())
			{
				currentRtti = currentRtti->baseRTTI;
				if (currentRtti == nullptr)
				{
					return nullptr;
				}
				it = editors.find(currentRtti);
			}
			return it->second;
		}
	}

	bool DispatchableNiObjectEditor(const char* label, RE::NiObject& object)
	{ 
		using namespace SNiObjectEditor;

		ImGui::PushID(label);

		bool wasEdited = false;

		const auto rtti = object.GetRTTI();
		const auto editor = DispatchEditor(*rtti);
		if (editor != nullptr)
		{
			const std::string name = GetFullName(object);
			if (PushingCollapsingHeader(name.c_str()))
			{
				wasEdited = editor(object);
				ImGui::TreePop();
			}
		}

		ImGui::PopID();

		return wasEdited;
	}
}
