#include "Gui/NiObjectEditor.h"

#include "Gui/NiTransformEditor.h"
#include "Gui/Utils.h"
#include "Utils/RTTICache.h"

#include <RE/B/BSGeometry.h>
#include <RE/B/BSLightingShaderMaterial.h>
#include <RE/B/BSLightingShaderProperty.h>
#include <RE/B/BSShaderTextureSet.h>
#include <RE/B/BSXFlags.h>
#include <RE/N/NiCollisionObject.h>
#include <RE/N/NiIntegerExtraData.h>
#include <RE/N/NiNode.h>
#include <RE/N/NiSkinInstance.h>
#include <RE/N/NiStringExtraData.h>
#include <RE/N/NiTimeController.h>

#include <imgui.h>

namespace SIE
{
	namespace SNiObjectEditor
	{
		static bool NiIntegerExtraDataEditor(void* object, void* context)
		{
			auto& extraData = *static_cast<RE::NiIntegerExtraData*>(object);

			bool wasEdited = false;

			if (ImGui::DragInt("Value", &extraData.value))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool BSXFlagsEditor(void* object, void* context) 
		{
			auto& bsxFlags = *static_cast<RE::BSXFlags*>(object);

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

		static bool NiStringExtraDataEditor(void* object, void* context)
		{
			auto& extraData = *static_cast<RE::NiStringExtraData*>(object);

			bool wasEdited = false;

			if (BSFixedStringEdit("Value", extraData.value))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		struct BSShaderTextureSetEditorContext
		{
			BSShaderTextureSetEditorContext(const RE::BSLightingShaderProperty& aProperty,
				const RE::BSLightingShaderMaterialBase& aMaterial) 
				: property(aProperty)
				, material(aMaterial)
			{}

			std::vector<std::pair<RE::BSTextureSet::Texture, std::string>> GetAvailableTextures()
			{
				std::vector<std::pair<RE::BSTextureSet::Texture, std::string>> result{
					{ RE::BSTextureSet::Texture::kDiffuse, "Diffuse" },
					{ RE::BSTextureSet::Texture::kNormal, "Normal" },
				};
				if (material.GetFeature() == RE::BSShaderMaterial::Feature::kEnvironmentMap)
				{
					result.push_back({RE::BSTextureSet::Texture::kEnvironment, "Environment Map"});
					result.push_back({RE::BSTextureSet::Texture::kEnvironmentMask, "Environment Mask"});
				}
				else if (material.GetFeature() == RE::BSShaderMaterial::Feature::kEye)
				{
					result.push_back(
						{ RE::BSTextureSet::Texture::kEnvironment, "Environment Map" });
					result.push_back(
						{ RE::BSTextureSet::Texture::kEnvironmentMask, "Environment Mask" });
				}
				else if (material.GetFeature() == RE::BSShaderMaterial::Feature::kFaceGen)
				{
					result.push_back(
						{ RE::BSTextureSet::Texture::kSubsurface, "Subsurface" });
					result.push_back({ RE::BSTextureSet::Texture::kDetail, "Detail" });
					result.push_back({ RE::BSTextureSet::Texture::kTint, "Tint" });
				}
				else if (material.GetFeature() == RE::BSShaderMaterial::Feature::kGlowMap)
				{
					result.push_back({ RE::BSTextureSet::Texture::kGlowMap, "Glowmap" });
				}
				else if (material.GetFeature() == RE::BSShaderMaterial::Feature::kMultilayerParallax)
				{
					result.push_back(
						{ RE::BSTextureSet::Texture::kEnvironment, "Environment Map" });
					result.push_back(
						{ RE::BSTextureSet::Texture::kEnvironmentMask, "Environment Mask" });
					result.push_back({ RE::BSTextureSet::Texture::kMultilayer, "Multilayer" });
				}
				else if (material.GetFeature() == RE::BSShaderMaterial::Feature::kParallax)
				{
					result.push_back({ RE::BSTextureSet::Texture::kHeight, "Height" });
				}

				if (property.flags.all(RE::BSShaderProperty::EShaderPropertyFlag::kBackLighting))
				{
					result.push_back(
						{ RE::BSTextureSet::Texture::kBacklightMask, "Backlight Mask" });
				}
				if (property.flags.all(RE::BSShaderProperty::EShaderPropertyFlag::kRimLighting))
				{
					result.push_back({ RE::BSTextureSet::Texture::kRimlightMask, "Rimlight Mask" });
				}
				else if (property.flags.all(
							 RE::BSShaderProperty::EShaderPropertyFlag::kSoftLighting))
				{
					result.push_back(
						{ RE::BSTextureSet::Texture::kSoftlightMask, "Softlight Mask" });
				}
				if (property.flags.all(RE::BSShaderProperty::EShaderPropertyFlag::kSpecular,
						RE::BSShaderProperty::EShaderPropertyFlag::kModelSpaceNormals))
				{
					result.push_back({ RE::BSTextureSet::Texture::kSpecular, "Specular" });
				}

				return result;
			}

			const RE::BSLightingShaderProperty& property;
			const RE::BSLightingShaderMaterialBase& material;
		};

		static bool BSShaderTextureSetEditor(void* object, void* contextPtr)
		{
			if (contextPtr == nullptr)
			{
				logger::error("BSShaderTextureSetEditorContext is not provided!");
			}

			auto& textureSet = *static_cast<RE::BSShaderTextureSet*>(object);
			auto& context = *static_cast<BSShaderTextureSetEditorContext*>(contextPtr);

			bool wasEdited = false;

			for (const auto& [textureIndex, textureName] : context.GetAvailableTextures())
			{
				if (NifTexturePathEdit(std::to_string(textureIndex).c_str(), textureName.c_str(),
						textureSet.textures[textureIndex]))
				{
					wasEdited = true;
				}
			}

			return wasEdited;
		}

		static bool NiObjectNETEditor(void* object, void* context) 
		{
			auto& objectNet = *static_cast<RE::NiObjectNET*>(object);

			bool wasEdited = false;

			if (objectNet.extraDataSize > 0)
			{
				for (uint16_t extraIndex = 0; extraIndex < objectNet.extraDataSize; ++extraIndex)
				{
					auto& rttiCache = RTTICache::Instance();
					const std::string& typeName =
						rttiCache.GetTypeName(objectNet.extra[extraIndex]);
					if (PushingCollapsingHeader(std::format("[Extra Data] <{}> {}##{}", typeName,
							objectNet.extra[extraIndex]->name.c_str(), extraIndex)
													.c_str()))
					{
						if (rttiCache.BuildEditor(objectNet.extra[extraIndex]))
						{
							wasEdited = true;
						}
						ImGui::TreePop();
					}
				}
			}

			if (objectNet.controllers != nullptr)
			{
				auto controller = objectNet.controllers.get();
				do {
					auto& rttiCache = RTTICache::Instance();
					const std::string& typeName = rttiCache.GetTypeName(controller);
					if (PushingCollapsingHeader(std::format("[Contoller] <{}>", typeName).c_str()))
					{
						if (rttiCache.BuildEditor(controller))
						{
							wasEdited = true;
						}
						ImGui::TreePop();
					}
					controller = controller->next.get();
				} while (controller != nullptr);
			}

			return wasEdited;
		}

		static bool NiAVObjectEditor(void* object, void* context) 
		{
			auto& avObject = *static_cast<RE::NiAVObject*>(object);

			bool wasEdited = false;

			if (avObject.collisionObject != nullptr)
			{
				auto& rttiCache = RTTICache::Instance();
				const std::string& typeName = rttiCache.GetTypeName(avObject.collisionObject.get());
				if (PushingCollapsingHeader(std::format("[Collision] <{}>", typeName).c_str()))
				{
					if (rttiCache.BuildEditor(avObject.collisionObject.get()))
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

		static bool NiNodeEditor(void* object, void* context) 
		{
			auto& node = *static_cast<RE::NiNode*>(object);

			bool wasEdited = false;

			if (!node.children.empty())
			{
				size_t childIndex = 0;
				for (auto& child : node.children)
				{
					if (child != nullptr)
					{
						auto& rttiCache = RTTICache::Instance();
						const std::string& typeName = rttiCache.GetTypeName(child.get());
						if (PushingCollapsingHeader(
								std::format("[Child] <{}> {}##{}", typeName, child->name.c_str(), childIndex)
									.c_str()))
						{
							if (rttiCache.BuildEditor(child.get()))
							{
								wasEdited = true;
							}
							ImGui::TreePop();
						}
						++childIndex;
					}
				}
			}

			return wasEdited;
		}

		static bool BSGeometryEditor(void* object, void* context)
		{
			auto& geometry = *static_cast<RE::BSGeometry*>(object);

			bool wasEdited = false;

			auto& rttiCache = RTTICache::Instance();

			size_t propertyIndex = 0;
			for (const auto& property : geometry.properties)
			{
				if (property != nullptr)
				{
					const std::string& typeName = rttiCache.GetTypeName(property.get());
					if (PushingCollapsingHeader(
							std::format("[Property] <{}>##{}", typeName, propertyIndex).c_str()))
					{
						if (rttiCache.BuildEditor(property.get()))
						{
							wasEdited = true;
						}
						ImGui::TreePop();
					}
					++propertyIndex;
				}
			}

			if (geometry.skinInstance != nullptr)
			{
				const std::string& typeName = rttiCache.GetTypeName(geometry.skinInstance.get());
				if (PushingCollapsingHeader(std::format("[Skin] <{}>", typeName).c_str()))
				{
					if (rttiCache.BuildEditor(geometry.skinInstance.get()))
					{
						wasEdited = true;
					}
					ImGui::TreePop();
				}
			}

			return wasEdited;
		}

		static bool BSShaderMaterialEditor(void* object, void* context) 
		{
			auto& material = *static_cast<RE::BSShaderMaterial*>(object);

			bool wasEdited = false;

			if (ImGui::DragFloat2("UV Offset 0", &material.texCoordOffset[0].x, 0.05f))
			{
				material.texCoordOffset[1] = material.texCoordOffset[0];
			}
			if (ImGui::DragFloat2("UV Scale 0", &material.texCoordScale[0].x, 0.05f))
			{
				material.texCoordScale[1] = material.texCoordScale[0];
			}

			return wasEdited;
		}

		struct BSShaderMaterialEditorContext
		{
			BSShaderMaterialEditorContext(const RE::BSShaderProperty& aProperty) :
				property(aProperty)
			{}

			const RE::BSShaderProperty& property;
		};

		static bool BSLightingShaderMaterialBaseEditor(void* object, void* contextPtr)
		{
			if (contextPtr == nullptr)
			{
				logger::error("BSShaderMaterialEditorContext is not provided!");
			}

			auto& material = *static_cast<RE::BSLightingShaderMaterialBase*>(object);
			auto& context = *static_cast<BSShaderMaterialEditorContext*>(contextPtr);

			bool wasEdited = false;

			auto& rttiCache = RTTICache::Instance();
			if (PushingCollapsingHeader(std::format("[Texture Set] <{}>",
					rttiCache.GetTypeName(material.textureSet.get()))
											.c_str()))
			{
				BSShaderTextureSetEditorContext textureSetContext{ static_cast<const RE::BSLightingShaderProperty&>(context.property), material };
				if (rttiCache.BuildEditor(material.textureSet.get(), &textureSetContext))
				{
					material.diffuseTexture = nullptr;
					material.OnLoadTextureSet(0, material.textureSet.get());
					wasEdited = true;
				}
				ImGui::TreePop();
			}

			if (context.property.flags.all(RE::BSLightingShaderProperty::EShaderPropertyFlag::kSpecular))
			{
				if (ImGui::ColorEdit3("Specular Color", &material.specularColor.red))
				{
					wasEdited = true;
				}
				if (ImGui::DragFloat("Specular Power", &material.specularPower, 0.01f))
				{
					wasEdited = true;
				}
				if (ImGui::DragFloat("Specular Color Scale", &material.specularColorScale, 0.01f))
				{
					wasEdited = true;
				}
			}
			if (ImGui::SliderFloat("Alpha", &material.materialAlpha, 0.f, 1.f))
			{
				wasEdited = true;
			}
			if (context.property.flags.any(
					RE::BSLightingShaderProperty::EShaderPropertyFlag::kRefraction,
					RE::BSLightingShaderProperty::EShaderPropertyFlag::kTempRefraction))
			{
				if (ImGui::DragFloat("Refraction Power", &material.refractionPower, 0.01f))
				{
					wasEdited = true;
				}
			}
			if (context.property.flags.any(
					RE::BSLightingShaderProperty::EShaderPropertyFlag::kRimLighting,
					RE::BSLightingShaderProperty::EShaderPropertyFlag::kSoftLighting))
			{
				if (ImGui::DragFloat("Subsurface Light Rolloff", &material.subSurfaceLightRolloff,
						0.01f))
				{
					wasEdited = true;
				}
				if (ImGui::DragFloat("Rim Light Power", &material.rimLightPower, 0.01f))
				{
					wasEdited = true;
				}
			}

			return wasEdited;
		}

		static bool BSShaderPropertyEditor(void* object, void* context)
		{
			auto& bsShaderProperty = *static_cast<RE::BSShaderProperty*>(object);

			bool wasEdited = false;

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
				auto& rttiCache = RTTICache::Instance();
				const std::string& typeName = rttiCache.GetTypeName(bsShaderProperty.material);
				if (PushingCollapsingHeader(std::format("[Material] <{}>", typeName).c_str()))
				{
					BSShaderMaterialEditorContext materialContext(bsShaderProperty);
					if (rttiCache.BuildEditor(bsShaderProperty.material, &materialContext))
					{
						wasEdited = true;
					}
					ImGui::TreePop();
				}
			}

			return wasEdited;
		}

		static bool BSLightingShaderPropertyEditor(void* object, void* context)
		{
			auto& bsLightingShaderProperty = *static_cast<RE::BSLightingShaderProperty*>(object);

			bool wasEdited = false;

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
	}

	bool DispatchableNiObjectEditor(const char* label, RE::NiObject& object)
	{ 
		using namespace SNiObjectEditor;

		ImGui::PushID(label);

		bool wasEdited = false;

		auto& rttiCache = RTTICache::Instance();
		const std::string name = GetFullName(object);
		if (PushingCollapsingHeader(name.c_str()))
		{
			wasEdited = rttiCache.BuildEditor(&object);
			ImGui::TreePop();
		}

		ImGui::PopID();

		return wasEdited;
	}

	void RegisterNiEditors() 
	{ 
		auto& rttiCache = RTTICache::Instance();
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiObjectNET),
			SNiObjectEditor::NiObjectNETEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiAVObject),
			SNiObjectEditor::NiAVObjectEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiNode),
			SNiObjectEditor::NiNodeEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSGeometry),
			SNiObjectEditor::BSGeometryEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSShaderProperty),
			SNiObjectEditor::BSShaderPropertyEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSLightingShaderProperty),
			SNiObjectEditor::BSLightingShaderPropertyEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSShaderMaterial),
			SNiObjectEditor::BSShaderMaterialEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSLightingShaderMaterialBase),
			SNiObjectEditor::BSLightingShaderMaterialBaseEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSXFlags),
			SNiObjectEditor::BSXFlagsEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiIntegerExtraData),
			SNiObjectEditor::NiIntegerExtraDataEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiStringExtraData),
			SNiObjectEditor::NiStringExtraDataEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSShaderTextureSet),
			SNiObjectEditor::BSShaderTextureSetEditor);
	}
}
