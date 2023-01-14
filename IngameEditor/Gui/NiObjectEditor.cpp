#include "Gui/NiObjectEditor.h"

#include "Gui/NiTransformEditor.h"
#include "Gui/Utils.h"
#include "Utils/RTTICache.h"

#include <RE/B/bhkCollisionObject.h>
#include <RE/B/bhkRigidBody.h>
#include <RE/B/BSFadeNode.h>
#include <RE/B/BSGeometry.h>
#include <RE/B/BSLightingShaderMaterialEnvmap.h>
#include <RE/B/BSLightingShaderMaterialEye.h>
#include <RE/B/BSLightingShaderMaterialFacegenTint.h>
#include <RE/B/BSLightingShaderMaterialHairTint.h>
#include <RE/B/BSLightingShaderMaterialMultilayerParallax.h>
#include <RE/B/BSLightingShaderMaterialParallaxOcc.h>
#include <RE/B/BSLightingShaderMaterialSnow.h>
#include <RE/B/BSLightingShaderProperty.h>
#include <RE/B/BSShaderTextureSet.h>
#include <RE/B/BSXFlags.h>
#include <RE/H/hkpRigidBody.h>
#include <RE/N/NiCollisionObject.h>
#include <RE/N/NiIntegerExtraData.h>
#include <RE/N/NiNode.h>
#include <RE/N/NiSkinInstance.h>
#include <RE/N/NiStream.h>
#include <RE/N/NiStringExtraData.h>
#include <RE/N/NiTimeController.h>

#include <imgui.h>

#include <type_traits>

namespace SIE
{
	template<typename RetT, typename BaseT, typename... ArgT>
	struct ToFunctionPointerHelper
	{
		ToFunctionPointerHelper(RetT (BaseT::*ptr)(ArgT...)){};

		using FunctionPointerType = RetT(*)(BaseT*, ArgT...);
	};

	template<typename T>
	using FunctionPointerType = typename decltype(ToFunctionPointerHelper(std::declval<T>()))::FunctionPointerType;

	namespace SNiObjectEditor
	{
		struct SaveData
		{
			std::optional<RE::NiTransform> topTransform;
		};

		SaveData PreSaveNiObject(RE::NiObject& object)
		{ 
			SaveData result;
			if (auto fadeNode = object.AsFadeNode())
			{
				result.topTransform = fadeNode->local;
				fadeNode->local = {};
			}
			return result;
		}

		void PostSaveNiObject(RE::NiObject& object, const SaveData& saveData)
		{
			if (auto fadeNode = object.AsFadeNode(); fadeNode != nullptr && saveData.topTransform.has_value())
			{
				fadeNode->local = *saveData.topTransform;
			}
		}

		static bool NiObjectEditor(void* object, void* context)
		{
			auto& niObject = *static_cast<RE::NiObject*>(object);

			bool wasEdited = false;

			static RE::BSFixedString savePath;
			FreeMeshPathEdit("SavePath", "", savePath);
			ImGui::SameLine();
			if (ImGui::Button("Save"))
			{
				auto stream = RE::NiStream::Create();
				const auto saveData = PreSaveNiObject(niObject);
				auto objectPtr = RE::NiPointer(&niObject);
				stream->topObjects.push_back(objectPtr);
				stream->Save3(savePath.c_str());
				PostSaveNiObject(niObject, saveData);
				stream->~NiStream();
				RE::free(stream);
			}

			return wasEdited;
		}

		static bool bhkRefObjectEditor(void* object, void* context)
		{
			auto& refObject = *static_cast<RE::bhkWorldObject*>(object);

			bool wasEdited = false;

			/*if (refObject.referencedObject != nullptr)
			{
				auto& rttiCache = RTTICache::Instance();
				const std::string& typeName = rttiCache.GetTypeName(refObject.referencedObject.get());
				if (PushingCollapsingHeader(std::format("[Referenced Object] <{}>", typeName).c_str()))
				{
					if (rttiCache.BuildEditor(refObject.referencedObject.get()))
					{
						wasEdited = true;
					}
					ImGui::TreePop();
				}
			}*/

			return wasEdited;
		}

		static bool bhkNiCollisionObjectEditor(void* object, void* context)
		{
			auto& collisionObject = *static_cast<RE::bhkNiCollisionObject*>(object);

			bool wasEdited = false;

			if (PushingCollapsingHeader("Flags"))
			{
				for (const auto& [value, name] :
					magic_enum::enum_entries<RE::bhkNiCollisionObject::Flag>())
				{
					if (value == RE::bhkNiCollisionObject::Flag::kNone)
					{
						continue;
					}
					if (FlagEdit(name.data(), collisionObject.flags, value))
					{
						wasEdited = true;
					}
				}
				ImGui::TreePop();
			}

			if (collisionObject.body != nullptr)
			{
				auto& rttiCache = RTTICache::Instance();
				const std::string& typeName = rttiCache.GetTypeName(collisionObject.body.get());
				if (PushingCollapsingHeader(std::format("[Body] <{}>", typeName).c_str()))
				{
					if (rttiCache.BuildEditor(collisionObject.body.get()))
					{
						wasEdited = true;
					}
					ImGui::TreePop();
				}
			}

			return wasEdited;
		}

		static bool bhkEntityEditor(void* object, void* context)
		{
			auto& entity = *static_cast<RE::bhkEntity*>(object);

			bool wasEdited = false;

			if (entity.referencedObject != nullptr)
			{
				auto havokEntity = static_cast<RE::hkpEntity*>(entity.referencedObject.get());

				float mass = 1.f / havokEntity->motion.inertiaAndMassInv.quad.m128_f32[3];
				if (ImGui::DragFloat("Mass", &mass, 0.1f, 0.f))
				{
					havokEntity->motion.SetMass(mass);
				}
				ImGui::DragFloat3("Inertia", havokEntity->motion.inertiaAndMassInv.quad.m128_f32,
					0.1f, 0.f);
				ImGui::DragFloat("Friction", &havokEntity->material.friction, 0.01f, 0.f);
				ImGui::DragFloat("Restitution", &havokEntity->material.restitution, 0.01f, 0.f);
				float rollingFrictionMultiplier = havokEntity->material.rollingFrictionMultiplier;
				if (ImGui::DragFloat("Rolling Friction Multiplier", &rollingFrictionMultiplier,
						0.01f, 0.f))
				{
					havokEntity->material.rollingFrictionMultiplier = rollingFrictionMultiplier;
				}
			}

			return wasEdited;
		}

		static bool bhkRigidBodyEditor(void* object, void* context)
		{
			auto& rigidBody = *static_cast<RE::bhkRigidBody*>(object);

			bool wasEdited = false;

			if (rigidBody.referencedObject != nullptr)
			{
				auto havokBody = static_cast<RE::hkpRigidBody*>(rigidBody.referencedObject.get());

				if (auto shape = havokBody->GetShape(); shape != nullptr && shape->userData != nullptr)
				{
					if (void* userData = reinterpret_cast<void*>(shape->userData))
					{
						auto& rttiCache = RTTICache::Instance();
						const std::string& typeName = rttiCache.GetTypeName(shape->userData);
						if (PushingCollapsingHeader(std::format("[Shape] <{}>", typeName).c_str()))
						{
							if (rttiCache.BuildEditor(shape->userData))
							{
								wasEdited = true;
							}
							ImGui::TreePop();
						}
					}
				}
			}

			if (!rigidBody.constraints.empty())
			{
				size_t constraintIndex = 0;
				for (auto& constraint : rigidBody.constraints)
				{
					if (constraint != nullptr)
					{
						auto& rttiCache = RTTICache::Instance();
						const std::string& typeName = rttiCache.GetTypeName(constraint.get());
						if (PushingCollapsingHeader(std::format("[Constraint] <{}>##{}", typeName, constraintIndex)
														.c_str()))
						{
							if (rttiCache.BuildEditor(constraint.get()))
							{
								wasEdited = true;
							}
							ImGui::TreePop();
						}
						++constraintIndex;
					}
				}
			}

			return wasEdited;
		}

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

		static bool BSLightingShaderMaterialEnvmapEditor(void* object, void* contextPtr)
		{
			auto& material = *static_cast<RE::BSLightingShaderMaterialEnvmap*>(object);

			bool wasEdited = false;

			if (ImGui::DragFloat("Envmap Scale", &material.envMapScale, 0.01f))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool BSLightingShaderMaterialEyeEditor(void* object, void* contextPtr)
		{
			auto& material = *static_cast<RE::BSLightingShaderMaterialEye*>(object);

			bool wasEdited = false;

			if (ImGui::DragFloat("Envmap Scale", &material.envMapScale, 0.01f))
			{
				wasEdited = true;
			}
			if (NiPoint3Editor("Left Eye Center", material.eyeCenter[0], 0.01f))
			{
				wasEdited = true;
			}
			if (NiPoint3Editor("Right Eye Center", material.eyeCenter[1], 0.01f))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool BSLightingShaderMaterialFacegenTintEditor(void* object, void* contextPtr)
		{
			auto& material = *static_cast<RE::BSLightingShaderMaterialFacegenTint*>(object);

			bool wasEdited = false;

			if (ImGui::ColorEdit3("Tint Color", &material.tintColor.red))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool BSLightingShaderMaterialHairTintEditor(void* object, void* contextPtr)
		{
			auto& material = *static_cast<RE::BSLightingShaderMaterialHairTint*>(object);

			bool wasEdited = false;

			if (ImGui::ColorEdit3("Tint Color", &material.tintColor.red))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool BSLightingShaderMaterialMultiLayerParallaxEditor(void* object, void* contextPtr)
		{
			auto& material = *static_cast<RE::BSLightingShaderMaterialMultiLayerParallax*>(object);

			bool wasEdited = false;

			if (ImGui::DragFloat("Layer Thickness", &material.parallaxLayerThickness, 0.01f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Refraction Scale", &material.parallaxRefractionScale, 0.01f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat2("Inner Layer UV Scale", &material.parallaxInnerLayerUScale, 0.01f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Envmap Scale", &material.envmapScale, 0.01f))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool BSLightingShaderMaterialParallaxOccEditor(void* object, void* contextPtr)
		{
			auto& material = *static_cast<RE::BSLightingShaderMaterialParallaxOcc*>(object);

			bool wasEdited = false;

			if (ImGui::DragFloat("Max Passes", &material.parallaxOccMaxPasses, 0.01f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Scale", &material.parallaxOccScale, 0.01f))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool BSLightingShaderMaterialSnowEditor(void* object, void* contextPtr)
		{
			auto& material = *static_cast<RE::BSLightingShaderMaterialSnow*>(object);

			bool wasEdited = false;

			if (ImGui::DragFloat4("Sparkle Params", &material.sparkleParams.red, 0.01f))
			{
				wasEdited = true;
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
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiObject),
			SNiObjectEditor::NiObjectEditor);
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
		rttiCache.RegisterEditor(
			*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSLightingShaderMaterialEnvmap),
			SNiObjectEditor::BSLightingShaderMaterialEnvmapEditor);
		rttiCache.RegisterEditor(
			*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSLightingShaderMaterialEye),
			SNiObjectEditor::BSLightingShaderMaterialEyeEditor);
		rttiCache.RegisterEditor(
			*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSLightingShaderMaterialFacegenTint),
			SNiObjectEditor::BSLightingShaderMaterialFacegenTintEditor);
		rttiCache.RegisterEditor(
			*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSLightingShaderMaterialHairTint),
			SNiObjectEditor::BSLightingShaderMaterialHairTintEditor);
		rttiCache.RegisterEditor(
			*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSLightingShaderMaterialMultiLayerParallax),
			SNiObjectEditor::BSLightingShaderMaterialMultiLayerParallaxEditor);
		rttiCache.RegisterEditor(
			*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSLightingShaderMaterialParallaxOcc),
			SNiObjectEditor::BSLightingShaderMaterialParallaxOccEditor);
		rttiCache.RegisterEditor(
			*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSLightingShaderMaterialSnow),
			SNiObjectEditor::BSLightingShaderMaterialSnowEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSXFlags),
			SNiObjectEditor::BSXFlagsEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiIntegerExtraData),
			SNiObjectEditor::NiIntegerExtraDataEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiStringExtraData),
			SNiObjectEditor::NiStringExtraDataEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSShaderTextureSet),
			SNiObjectEditor::BSShaderTextureSetEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_bhkRefObject),
			SNiObjectEditor::bhkRefObjectEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_bhkNiCollisionObject),
			SNiObjectEditor::bhkNiCollisionObjectEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_bhkEntity),
			SNiObjectEditor::bhkEntityEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_bhkRigidBody),
			SNiObjectEditor::bhkRigidBodyEditor);
	}
}
