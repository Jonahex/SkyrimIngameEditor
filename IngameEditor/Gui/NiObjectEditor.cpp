#include "Gui/NiObjectEditor.h"

#include "Gui/NiTransformEditor.h"
#include "Gui/Utils.h"
#include "Utils/RTTICache.h"

#include <RE/B/bhkCollisionObject.h>
#include <RE/B/bhkRigidBody.h>
#include <RE/B/BSEffectShaderMaterial.h>
#include <RE/B/BSFadeNode.h>
#include <RE/B/BSFurnitureMarkerNode.h>
#include <RE/B/BSInvMarker.h>
#include <RE/B/BSLagBoneController.h>
#include <RE/B/BSLightingShaderMaterialEnvmap.h>
#include <RE/B/BSLightingShaderMaterialEye.h>
#include <RE/B/BSLightingShaderMaterialFacegenTint.h>
#include <RE/B/BSLightingShaderMaterialHairTint.h>
#include <RE/B/BSLightingShaderMaterialMultilayerParallax.h>
#include <RE/B/BSLightingShaderMaterialParallaxOcc.h>
#include <RE/B/BSLightingShaderMaterialSnow.h>
#include <RE/B/BSLightingShaderProperty.h>
#include <RE/B/BSPSysArrayEmitter.h>
#include <RE/B/BSPSysHavokUpdateModifier.h>
#include <RE/B/BSPSysInheritVelocityModifier.h>
#include <RE/B/BSPSysLODModifier.h>
#include <RE/B/BSPSysRecycleBoundModifier.h>
#include <RE/B/BSPSysScaleModifier.h>
#include <RE/B/BSPSysSimpleColorModifier.h>
#include <RE/B/BSPSysStripUpdateModifier.h>
#include <RE/B/BSPSysSubTexModifier.h>
#include <RE/B/BSShaderTextureSet.h>
#include <RE/B/BSTriShape.h>
#include <RE/B/BSWindModifier.h>
#include <RE/B/BSXFlags.h>
#include <RE/H/hkpRigidBody.h>
#include <RE/N/NiAlphaProperty.h>
#include <RE/N/NiBillboardNode.h>
#include <RE/N/NiCollisionObject.h>
#include <RE/N/NiControllerManager.h>
#include <RE/N/NiControllerSequence.h>
#include <RE/N/NiIntegerExtraData.h>
#include <RE/N/NiParticleSystem.h>
#include <RE/N/NiPSysAgeDeathModifier.h>
#include <RE/N/NiPSysAirFieldModifier.h>
#include <RE/N/NiPSysBombModifier.h>
#include <RE/N/NiPSysBoundUpdateModifier.h>
#include <RE/N/NiPSysBoxEmitter.h>
#include <RE/N/NiPSysCollider.h>
#include <RE/N/NiPSysColliderManager.h>
#include <RE/N/NiPSysColorModifier.h>
#include <RE/N/NiPSysCylinderEmitter.h>
#include <RE/N/NiPSysDragFieldModifier.h>
#include <RE/N/NiPSysDragModifier.h>
#include <RE/N/NiPSysGravityFieldModifier.h>
#include <RE/N/NiPSysGravityModifier.h>
#include <RE/N/NiPSysGrowFadeModifier.h>
#include <RE/N/NiPSysMeshEmitter.h>
#include <RE/N/NiPSysModifierCtlr.h>
#include <RE/N/NiPSysPlanarCollider.h>
#include <RE/N/NiPSysPositionModifier.h>
#include <RE/N/NiPSysRadialFieldModifier.h>
#include <RE/N/NiPSysRotationModifier.h>
#include <RE/N/NiPSysSpawnModifier.h>
#include <RE/N/NiPSysSphereEmitter.h>
#include <RE/N/NiPSysSphericalCollider.h>
#include <RE/N/NiPSysTurbulenceFieldModifier.h>
#include <RE/N/NiPSysVortexFieldModifier.h>
#include <RE/N/NiRTTI.h>
#include <RE/N/NiSkinInstance.h>
#include <RE/N/NiSingleInterpController.h>
#include <RE/N/NiStream.h>
#include <RE/N/NiStringExtraData.h>
#include <RE/N/NiTStringMap.h>

#include <imgui.h>

#include <numbers>
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
		struct NiObjectContext
		{
			NiObjectContext(RE::NiObject& aRoot) 
				: root(aRoot)
				, streamablesProvider(RE::NiStream::Create())
			{ 
				aRoot.RegisterStreamables(*streamablesProvider);
			}

			~NiObjectContext()
			{ 
				RE::free(streamablesProvider);
			}

			RE::NiObject& root;
			RE::NiStream* streamablesProvider;
		};

		bool NiObjectTypeSelector(const char* label, const TypeDescriptor& base, const RTTI*& rtti)
		{
			static std::unordered_map<const TypeDescriptor*, std::vector<const RTTI*>> descendantsCache;

			auto descendantsIt = descendantsCache.find(&base);
			if (descendantsIt == descendantsCache.cend())
			{
				auto descendants = RTTICache::Instance().GetConstructibleDescendants(base);
				std::sort(descendants.begin(), descendants.end(),
					[](auto first, auto second)
					{
						return std::lexicographical_compare(first->typeName.begin(),
							first->typeName.end(), second->typeName.begin(),
							second->typeName.end());
					});
				descendantsIt = descendantsCache.insert_or_assign(&base, std::move(descendants)).first;
			}

			bool wasSelected = false;
			if (ImGui::BeginCombo(label, rtti == nullptr ? "None" : rtti->typeName.c_str()))
			{
				for (const auto& descendantRtti : descendantsIt->second)
				{
					const bool isSelected = descendantRtti == rtti;
					if (ImGui::Selectable(descendantRtti->typeName.c_str(), isSelected))
					{
						rtti = descendantRtti;
						wasSelected = true;
					}
				}
				ImGui::EndCombo();
			}
			return wasSelected;
		}

		template <typename BaseType>
		bool TargetSelector(const char* label, const NiObjectContext& context,
			BaseType*& target, const std::function<std::string_view(const BaseType*)>& nameProvider,
			const std::function<bool(BaseType*)>& filter = nullptr) 
		{
			bool wasSelected = false;
			if (ImGui::BeginCombo(label, target == nullptr ? "None" : nameProvider(target).data()))
			{
				std::vector<BaseType*> possibleTargets{ nullptr };
				for (auto item : context.streamablesProvider->objects)
				{
					if (auto casted = netimmerse_cast<BaseType*>(item.get()))
					{
						if (filter(casted))
						{
							possibleTargets.push_back(casted);
						}
					}
				}

				for (auto possibleTarget : possibleTargets)
				{
					const bool isSelected = possibleTarget == target;
					if (ImGui::Selectable(possibleTarget == nullptr ? "None" : nameProvider(possibleTarget).data(),
							isSelected))
					{
						target = possibleTarget;
						wasSelected = true;
					}
				}
				ImGui::EndCombo();
			}
			return wasSelected;
		}

		template<typename T>
		bool NiObjectNETTargetSelector(const char* label, const NiObjectContext& context,
			T*& target)
		{
			return TargetSelector<T>(label, context, target,
				[](const T* objectNet) { return std::string_view(objectNet->name.data());
				});
		}

		template <typename T>
		bool NiPSysModifierTargetSelector(const char* label, const NiObjectContext& context,
			T*& target, const std::function<bool(T*)>& filter = nullptr)
		{
			return TargetSelector<T>(label, context, target,
				[](const T* modifier) { return std::string_view(modifier->name.data()); }, filter);
		}

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
					if (rttiCache.BuildEditor(collisionObject.body.get(), context))
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
							if (rttiCache.BuildEditor(shape->userData, context))
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
							if (rttiCache.BuildEditor(constraint.get(), context))
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

		struct BSShaderTextureSetEditorContext : public NiObjectContext
		{
			BSShaderTextureSetEditorContext(RE::NiObject& aRoot, const RE::BSLightingShaderProperty& aProperty,
				const RE::BSLightingShaderMaterialBase& aMaterial) 
				: NiObjectContext(aRoot)
				, property(aProperty)
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

			if (BSFixedStringEdit("Name", objectNet.name))
			{
				wasEdited = true;
			}

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
						if (ImGui::Button("Remove"))
						{
							objectNet.RemoveExtraDataAt(extraIndex);
							break;
						}
						if (rttiCache.BuildEditor(objectNet.extra[extraIndex], context))
						{
							wasEdited = true;
						}
						ImGui::TreePop();
					}
				}
			}

			{
				static const auto extraDataTypeDescriptor =
					&*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiExtraData);
				static const RTTI* newExtraDataRtti = nullptr;

				NiObjectTypeSelector("##NewExtraData", *extraDataTypeDescriptor, newExtraDataRtti);
				ImGui::SameLine();
				if (ImGui::Button("Add Extra Data"))
				{
					if (newExtraDataRtti != nullptr)
					{
						auto newExtraData =
							static_cast<RE::NiExtraData*>(newExtraDataRtti->constructor());
						if (newExtraData != nullptr)
						{
							objectNet.AddExtraData(newExtraData);
						}
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
						if (rttiCache.BuildEditor(controller, context))
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
					if (rttiCache.BuildEditor(avObject.collisionObject.get(), context))
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
							if (ImGui::Button("Remove"))
							{
								node.DetachChild(child.get());
								wasEdited = true;
								break;
							}
							if (rttiCache.BuildEditor(child.get(), context))
							{
								wasEdited = true;
							}
							ImGui::TreePop();
						}
						++childIndex;
					}
				}
			}

			{
				static const auto childTypeDescriptor =
					&*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiAVObject);
				static const RTTI* newNodeRtti = nullptr;

				NiObjectTypeSelector("##NewChild", *childTypeDescriptor, newNodeRtti);
				ImGui::SameLine();
				if (ImGui::Button("Add Child"))
				{
					if (newNodeRtti != nullptr)
					{
						auto newChild = static_cast<RE::NiAVObject*>(newNodeRtti->constructor());
						if (newChild != nullptr)
						{
							node.AttachChild(newChild);
						}
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

			if (PushingCollapsingHeader("Vertex Descriptor"))
			{
				for (const auto& [value, name] : magic_enum::enum_entries<RE::BSGraphics::Vertex::Flags>())
				{
					bool isPresent = geometry.vertexDesc.HasFlag(value);
					ImGui::Checkbox(name.data(), &isPresent);
				}

				ImGui::TreePop();
			}

			size_t propertyIndex = 0;
			for (const auto& property : geometry.properties)
			{
				if (property != nullptr)
				{
					const std::string& typeName = rttiCache.GetTypeName(property.get());
					if (PushingCollapsingHeader(
							std::format("[Property] <{}>##{}", typeName, propertyIndex).c_str()))
					{
						if (rttiCache.BuildEditor(property.get(), context))
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
					if (rttiCache.BuildEditor(geometry.skinInstance.get(), context))
					{
						wasEdited = true;
					}
					ImGui::TreePop();
				}
			}

			return wasEdited;
		}

		static bool BSTriShapeEditor(void* object, void* context)
		{
			auto& triShape = *static_cast<RE::BSTriShape*>(object);

			bool wasEdited = false;

			ImGui::Text("%d triangles", triShape.triangleCount);
			ImGui::Text("%d vertices", triShape.vertexCount);

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

		struct BSShaderMaterialEditorContext : public NiObjectContext
		{
			BSShaderMaterialEditorContext(RE::NiObject& aRoot, const RE::BSShaderProperty& aProperty) 
				: NiObjectContext(aRoot)
				, property(aProperty)
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
				BSShaderTextureSetEditorContext textureSetContext{ reinterpret_cast<NiObjectContext*>(contextPtr)->root, static_cast<const RE::BSLightingShaderProperty&>(context.property), material };
				if (rttiCache.BuildEditor(material.textureSet.get(), &textureSetContext))
				{
					material.diffuseTexture = nullptr;
					material.OnLoadTextureSet(0, material.textureSet.get());
					wasEdited = true;
				}
				ImGui::TreePop();
			}

			if (EnumSelector("Texture Clamp Mode", material.textureClampMode))
			{
				wasEdited = true;
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
			auto& shaderProperty = *static_cast<RE::BSShaderProperty*>(object);

			bool wasEdited = false;

			if (PushingCollapsingHeader("Flags"))
			{
				for (const auto& [value, name] :
					magic_enum::enum_entries<RE::BSShaderProperty::EShaderPropertyFlag>())
				{
					if (FlagEdit(name.data(), shaderProperty.flags, value))
					{
						wasEdited = true;
					}
				}
				ImGui::TreePop();
			}

			if (shaderProperty.material != nullptr)
			{
				auto& rttiCache = RTTICache::Instance();
				const std::string& typeName = rttiCache.GetTypeName(shaderProperty.material);
				if (PushingCollapsingHeader(std::format("[Material] <{}>", typeName).c_str()))
				{
					BSShaderMaterialEditorContext materialContext(
						reinterpret_cast<NiObjectContext*>(context)->root, shaderProperty);
					if (rttiCache.BuildEditor(shaderProperty.material, &materialContext))
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
			auto& lightingShaderProperty = *static_cast<RE::BSLightingShaderProperty*>(object);

			bool wasEdited = false;

			if (lightingShaderProperty.emissiveColor != nullptr)
			{
				if (ImGui::ColorEdit3("Emissive Color",
						&lightingShaderProperty.emissiveColor->red))
				{
					wasEdited = true;
				}
			}

			if (ImGui::DragFloat("Emissive Multiplier", &lightingShaderProperty.emissiveMult, 0.01f, 0.f))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiAlphaPropertyEditor(void* object, void* context)
		{
			auto& alphaProperty = *static_cast<RE::NiAlphaProperty*>(object);

			bool wasEdited = false;

			{
				constexpr uint8_t minValue = 0;
				constexpr uint8_t maxValue = 255;
				if (ImGui::SliderScalar("Alpha Threshold", ImGuiDataType_U8,
						&alphaProperty.alphaThreshold, &minValue, &maxValue))
				{
					wasEdited = true;
				}
			}

			{
				bool blendingEnabled = alphaProperty.GetAlphaBlending();
				if (ImGui::Checkbox("Enabled Blending", &blendingEnabled))
				{
					alphaProperty.SetAlphaBlending(blendingEnabled);
					wasEdited = true;
				}
			}

			{
				auto srcBlendMode = alphaProperty.GetSrcBlendMode();
				if (EnumSelector<RE::NiAlphaProperty::AlphaFunction>("Source Blend Mode", srcBlendMode))
				{
					alphaProperty.SetSrcBlendMode(srcBlendMode);
					wasEdited = true;
				}
			}

			{
				auto dstBlendMode = alphaProperty.GetDestBlendMode();
				if (EnumSelector<RE::NiAlphaProperty::AlphaFunction>("Destination Blend Mode",
						dstBlendMode))
				{
					alphaProperty.SetDestBlendMode(dstBlendMode);
					wasEdited = true;
				}
			}

			{
				bool testingEnabled = alphaProperty.GetAlphaTesting();
				if (ImGui::Checkbox("Enabled Testing", &testingEnabled))
				{
					alphaProperty.SetAlphaTesting(testingEnabled);
					wasEdited = true;
				}
			}

			{
				auto alphaTestMode = alphaProperty.GetAlphaTestMode();
				if (EnumSelector<RE::NiAlphaProperty::TestFunction>("Alpha Test Function",
						alphaTestMode))
				{
					alphaProperty.SetAlphaTestMode(alphaTestMode);
					wasEdited = true;
				}
			}

			{
				bool noSorter = alphaProperty.GetNoSorter();
				if (ImGui::Checkbox("No Sorter", &noSorter))
				{
					alphaProperty.SetNoSorter(noSorter);
					wasEdited = true;
				}
			}

			return wasEdited;
		}

		static bool NiParticlesEditor(void* object, void* context)
		{
			auto& particles = *static_cast<RE::NiParticles*>(object);

			bool wasEdited = false;

			auto& rttiCache = RTTICache::Instance();
			if (particles.particleData != nullptr)
			{
				const std::string& typeName = rttiCache.GetTypeName(particles.particleData.get());
				if (PushingCollapsingHeader(
						std::format("[Particles Data] <{}>", typeName).c_str()))
				{
					if (rttiCache.BuildEditor(particles.particleData.get(), context))
					{
						wasEdited = true;
					}
					ImGui::TreePop();
				}
			}

			return wasEdited;
		}

		static bool NiParticleSystemEditor(void* object, void* context)
		{
			auto& particleSystem = *static_cast<RE::NiParticleSystem*>(object);

			bool wasEdited = false;

			if (ImGui::Checkbox("World Space", &particleSystem.isWorldspace))
			{
				wasEdited = true;
			}

			auto& rttiCache = RTTICache::Instance();

			{
				size_t modifierIndex = 0;
				for (auto it = particleSystem.modifierList.begin();
					it != particleSystem.modifierList.end(); ++it)
				{
					if (*it != nullptr)
					{
						const std::string& typeName = rttiCache.GetTypeName(it->get());
						if (PushingCollapsingHeader(std::format("[Modifier] <{}> {}##{}", typeName,
								(*it)->name.c_str(), modifierIndex)
									.c_str()))
						{
							if (rttiCache.BuildEditor(it->get(), context))
							{
								wasEdited = true;
							}
							ImGui::TreePop();
						}
					}
				}
			}

			{
				static const auto modifierTypeDescriptor =
					&*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysModifier);
				static const RTTI* newModifierRtti = nullptr;

				NiObjectTypeSelector("##NewModifier", *modifierTypeDescriptor, newModifierRtti);
				ImGui::SameLine();
				if (ImGui::Button("Add Modifier"))
				{
					if (newModifierRtti != nullptr)
					{
						auto newModifier =
							static_cast<RE::NiPSysModifier*>(newModifierRtti->constructor());
						if (newModifier != nullptr)
						{
							particleSystem.AddModifier(newModifier);
						}
					}
				}
			}

			return wasEdited;
		}

		static bool NiBillboardNodeEditor(void* object, void* context)
		{
			auto& billboardNode = *static_cast<RE::NiBillboardNode*>(object);

			bool wasEdited = false;

			auto mode = REX::EnumSet(billboardNode.GetMode());
			if (EnumSelector("Billboard Mode", mode))
			{
				billboardNode.SetMode(mode.get());
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiTimeControllerEditor(void* object, void* context)
		{
			auto& timeController = *static_cast<RE::NiTimeController*>(object);

			bool wasEdited = false;

			uint16_t animType = timeController.flags.underlying() & 0b1;
			if (EnumSelector<RE::NiTimeController::AnimType>("Anim Type", animType))
			{
				timeController.flags = 
					static_cast<RE::NiTimeController::Flag>(
					(animType & 0b1) | (timeController.flags.underlying() & ~0b1));
				wasEdited = true;
			}

			uint16_t cycleType = (timeController.flags.underlying() >> 1) & 0b11;
			if (EnumSelector<RE::NiTimeController::CycleType>("Cycle Type", cycleType))
			{
				timeController.flags = static_cast<RE::NiTimeController::Flag>(
					((cycleType & 0b11) << 1) | (timeController.flags.underlying() & ~0b110));
				wasEdited = true;
			}

			for (size_t flagIndex = 3; flagIndex < 8; ++flagIndex)
			{
				const auto flagMask = static_cast<RE::NiTimeController::Flag>(1 << flagIndex);
				if (FlagEdit(magic_enum::enum_name(flagMask).data(), timeController.flags,
						flagMask))
				{
					wasEdited = true;
				}
			}

			if (ImGui::DragFloat("Frequency", &timeController.frequency, 0.1f, 0.f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Phase", &timeController.phase, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Start Time", &timeController.loKeyTime, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("End Time", &timeController.hiKeyTime, 0.1f))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiControllerManagerEditor(void* object, void* context)
		{
			auto& controllerManager = *static_cast<RE::NiControllerManager*>(object);

			bool wasEdited = false;

			if (ImGui::Checkbox("Cumulative", &controllerManager.cumulative))
			{
				wasEdited = true;
			}

			auto& rttiCache = RTTICache::Instance();

			{
				size_t sequenceIndex = 0;

				for (const auto& controllerSequence : controllerManager.sequenceArray)
				{
					if (controllerSequence != nullptr)
					{
						const std::string& typeName =
							rttiCache.GetTypeName(controllerSequence.get());
						if (PushingCollapsingHeader(std::format("[Contoller Sequence] <{}> {}##{}",
								typeName, controllerSequence->name.c_str(), sequenceIndex)
														.c_str()))
						{
							if (rttiCache.BuildEditor(controllerSequence.get(), context))
							{
								wasEdited = true;
							}
							ImGui::TreePop();
						}
					}
				}
			}

			if (controllerManager.objectPalette != nullptr)
			{
				const std::string& typeName =
					rttiCache.GetTypeName(controllerManager.objectPalette.get());
				if (PushingCollapsingHeader(std::format("[Object Palette] <{}>", typeName).c_str()))
				{
					if (rttiCache.BuildEditor(controllerManager.objectPalette.get(), context))
					{
						wasEdited = true;
					}
					ImGui::TreePop();
				}
			}

			return wasEdited;
		}

		static bool BSLagBoneControllerEditor(void* object, void* context)
		{
			auto& lagBoneController = *static_cast<RE::BSLagBoneController*>(object);

			bool wasEdited = false;

			if (ImGui::DragFloat("Linear Velocity", &lagBoneController.linearVelocity))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Linear Rotation", &lagBoneController.linearRotation))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Maximum Distance", &lagBoneController.maximumDistance))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiSingleInterpControllerEditor(void* object, void* context)
		{
			auto& singleInterpController = *static_cast<RE::NiSingleInterpController*>(object);

			bool wasEdited = false;

			auto& rttiCache = RTTICache::Instance();

			if (singleInterpController.interpolator != nullptr)
			{
				const std::string& typeName =
					rttiCache.GetTypeName(singleInterpController.interpolator.get());
				if (PushingCollapsingHeader(std::format("[Interpolator] <{}>", typeName).c_str()))
				{
					if (rttiCache.BuildEditor(singleInterpController.interpolator.get(), context))
					{
						wasEdited = true;
					}
					ImGui::TreePop();
				}
			}

			return wasEdited;
		}

		static bool BSFurnitureMarkerNodeEditor(void* object, void* context)
		{
			auto& furnitureMarkerNode = *static_cast<RE::BSFurnitureMarkerNode*>(object);

			bool wasEdited = false;

			size_t markerIndex = 0;
			for (auto& marker : furnitureMarkerNode.markers)
			{
				if (NiPoint3Editor(std::format("Offset##{}", markerIndex).c_str(), marker.offset))
				{
					wasEdited = true;
				}
				if (ImGui::SliderFloat(std::format("Heading##{}", markerIndex).c_str(),
						&marker.heading, -std::numbers::pi, std::numbers::pi))
				{
					wasEdited = true;
				}
				if (EnumSelector(std::format("Animation Type##{}", markerIndex).c_str(),
						marker.animationType))
				{
					wasEdited = true;
				}
				if (EnumSelector(std::format("Entry Type##{}", markerIndex).c_str(),
						marker.entryProperties))
				{
					wasEdited = true;
				}
			}

			return wasEdited;
		}

		static bool BSInvMarkerEditor(void* object, void* context)
		{
			auto& invMarker = *static_cast<RE::BSInvMarker*>(object);

			bool wasEdited = false;

			if (ImGui::DragFloat("Zoom", &invMarker.zoom, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragScalarN("Rotation", ImGuiDataType_U16, &invMarker.rotationX, 3))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiPSysModifierEditor(void* object, void* context)
		{
			auto& modifier = *static_cast<RE::NiPSysModifier*>(object);

			bool wasEdited = false;

			if (BSFixedStringEdit("Name", modifier.name))
			{
				wasEdited = true;
			}
			{
				auto target = modifier.target;
				if (NiObjectNETTargetSelector<RE::NiParticleSystem>("Target",
						*reinterpret_cast<NiObjectContext*>(context), target))
				{
					modifier.SetSystemPointer(target);
					wasEdited = true;
				}
			}
			if (EnumSelector("Order", modifier.order))
			{
				wasEdited = true;
			}
			if (ImGui::Checkbox("Active", &modifier.active))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiPSysGravityModifierEditor(void* object, void* context)
		{
			auto& gravityModifier = *static_cast<RE::NiPSysGravityModifier*>(object);

			bool wasEdited = false;

			if (NiObjectNETTargetSelector<RE::NiAVObject>("Gravity Object",
					*reinterpret_cast<NiObjectContext*>(context), gravityModifier.gravityObj))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Decay", &gravityModifier.decay, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Strength", &gravityModifier.strength, 0.1f))
			{
				wasEdited = true;
			}
			if (EnumSelector("Force Type", gravityModifier.forceType))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Turbulence", &gravityModifier.turbulence, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Scale", &gravityModifier.scale, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::Checkbox("World Aligned", &gravityModifier.worldAligned))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiPSysEmitterEditor(void* object, void* context)
		{
			auto& emitter = *static_cast<RE::NiPSysEmitter*>(object);

			bool wasEdited = false;

			if (ImGui::DragFloat("Speed", &emitter.speed, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Speed Variation", &emitter.speedVariation, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::SliderFloat("Declination", &emitter.declination, 0.f, 2 * std::numbers::pi_v<float>))
			{
				wasEdited = true;
			}
			if (ImGui::SliderFloat("Declination Variation", &emitter.declinationVariation, 0.f,
					2 * std::numbers::pi_v<float>))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Planar Angle", &emitter.planarAngle, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Planar Angle Variation", &emitter.planarAngleVariation, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::ColorEdit4("Initial Color", &emitter.initialColor.red))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Initial Radius", &emitter.initialRadius, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Radius Variation", &emitter.radiusVariation, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Life Span", &emitter.lifeSpan, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Life Span Variation", &emitter.lifeSpanVariation, 0.1f))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiPSysVolumeEmitterEditor(void* object, void* context)
		{
			auto& volumeEmitter = *static_cast<RE::NiPSysVolumeEmitter*>(object);

			bool wasEdited = false;

			if (NiObjectNETTargetSelector<RE::NiNode>("Emitter Object",
					*reinterpret_cast<NiObjectContext*>(context), volumeEmitter.emitter))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiPSysCylinderEmitterEditor(void* object, void* context)
		{
			auto& cylinderEmitter = *static_cast<RE::NiPSysCylinderEmitter*>(object);

			bool wasEdited = false;

			if (ImGui::DragFloat("Radius", &cylinderEmitter.radius, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Height", &cylinderEmitter.height, 0.1f))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiPSysBoxEmitterEditor(void* object, void* context)
		{
			auto& boxEmitter = *static_cast<RE::NiPSysBoxEmitter*>(object);

			bool wasEdited = false;

			if (ImGui::DragFloat("Width", &boxEmitter.width, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Height", &boxEmitter.height, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Depth", &boxEmitter.depth, 0.1f))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiPSysSphereEmitterEditor(void* object, void* context)
		{
			auto& sphereEmitter = *static_cast<RE::NiPSysSphereEmitter*>(object);

			bool wasEdited = false;

			if (ImGui::DragFloat("Radius", &sphereEmitter.radius, 0.1f))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiPSysAgeDeathModifierEditor(void* object, void* context)
		{
			auto& ageDeathModifier = *static_cast<RE::NiPSysAgeDeathModifier*>(object);

			bool wasEdited = false;

			if (ImGui::Checkbox("Spawn On Death", &ageDeathModifier.spawnOnDeath))
			{
				wasEdited = true;
			}
			if (NiPSysModifierTargetSelector<RE::NiPSysSpawnModifier>("Spawn Modifier",
					*reinterpret_cast<NiObjectContext*>(context), ageDeathModifier.spawnModifier))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiPSysSpawnModifierEditor(void* object, void* context)
		{
			auto& spawnModifier = *static_cast<RE::NiPSysSpawnModifier*>(object);

			bool wasEdited = false;

			if (ImGui::DragScalar("Spawn Generations", ImGuiDataType_U16, &spawnModifier.numSpawnGenerations))
			{
				wasEdited = true;
			}
			if (ImGui::SliderFloat("Spawn Probability", &spawnModifier.spawnProbability, 0.f, 1.f))
			{
				wasEdited = true;
			}
			if (ImGui::DragScalarN("Spawn Number Range", ImGuiDataType_U16,
					&spawnModifier.minNumToSpawn, 2))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Spawn Speed Variation", &spawnModifier.spawnSpeedVariation, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Spawn Direction Variation", &spawnModifier.spawnDirVariation, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Life Span", &spawnModifier.lifeSpan,
					0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Life Span Variation", &spawnModifier.lifeSpanVariation,
					0.1f))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiPSysMeshEmitterEditor(void* object, void* context)
		{
			auto& meshEmitter = *static_cast<RE::NiPSysMeshEmitter*>(object);

			bool wasEdited = false;

			{
				size_t emitterMeshIndex = 0;
				for (auto& emitterMesh : meshEmitter.emitterMeshes)
				{
					auto emitterMeshPtr = emitterMesh.get();
					if (NiObjectNETTargetSelector<RE::BSTriShape>(std::format("Emitter Mesh##{}", emitterMeshIndex).c_str(),
							*reinterpret_cast<NiObjectContext*>(context), emitterMeshPtr))
					{
						emitterMesh = RE::NiPointer(emitterMeshPtr);
						wasEdited = true;
					}
					++emitterMeshIndex;
				}
			}

			if (EnumSelector("Velocity Type", meshEmitter.initialVelocityType))
			{
				wasEdited = true;
			}
			if (EnumSelector("Emission Type", meshEmitter.emissionType))
			{
				wasEdited = true;
			}
			if (NiPoint3Editor("Emission Axis", meshEmitter.emissionAxis, 0.1f))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiPSysBombModifierEditor(void* object, void* context)
		{
			auto& bombModifier = *static_cast<RE::NiPSysBombModifier*>(object);

			bool wasEdited = false;

			if (NiObjectNETTargetSelector<RE::NiNode>("Bomb Object",
					*reinterpret_cast<NiObjectContext*>(context), bombModifier.bombObject))
			{
				wasEdited = true;
			}
			if (NiPoint3Editor("Bomb Axis", bombModifier.bombAxis, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Decay", &bombModifier.decay, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Acceleration", &bombModifier.acceleration, 0.1f))
			{
				wasEdited = true;
			}
			if (EnumSelector("Decay Type", bombModifier.decayType))
			{
				wasEdited = true;
			}
			if (EnumSelector("Symmetry Type", bombModifier.symmetryType))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiPSysDragModifierEditor(void* object, void* context)
		{
			auto& dragModifier = *static_cast<RE::NiPSysDragModifier*>(object);

			bool wasEdited = false;

			if (NiObjectNETTargetSelector<RE::NiAVObject>("Drag Object",
					*reinterpret_cast<NiObjectContext*>(context), dragModifier.dragObject))
			{
				wasEdited = true;
			}
			if (NiPoint3Editor("Drag Axis", dragModifier.dragAxis, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::SliderFloat("Percentage", &dragModifier.percentage, 0.f, 1.f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Range", &dragModifier.range, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Range Falloff", &dragModifier.rangeFalloff, 0.1f))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiPSysRotationModifierEditor(void* object, void* context)
		{
			auto& rotationModifier = *static_cast<RE::NiPSysRotationModifier*>(object);

			bool wasEdited = false;

			if (ImGui::DragFloat("Angular Speed", &rotationModifier.rotationSpeed, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Angular Speed Variation",
					&rotationModifier.rotationSpeedVariation, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Initial Angle", &rotationModifier.rotationAngle, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Initial Angle Variation",
					&rotationModifier.rotationAngleVariation, 0.1f))
			{
				wasEdited = true;
			}
			if (NiPoint3Editor("Rotation Axis", rotationModifier.rotationAxis, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::Checkbox("Random Rotation Axis", &rotationModifier.randomRotationAxis))
			{
				wasEdited = true;
			}
			if (ImGui::Checkbox("Random Angular Speed Sign", &rotationModifier.randomRotationSpeedSign))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiPSysBoundUpdateModifierEditor(void* object, void* context)
		{
			auto& boundUpdateModifier = *static_cast<RE::NiPSysBoundUpdateModifier*>(object);

			bool wasEdited = false;

			{
				auto updateSkip = boundUpdateModifier.updateSkip;
				if (ImGui::DragScalar("Update Skip", ImGuiDataType_U16, &updateSkip))
				{
					boundUpdateModifier.SetUpdateSkip(updateSkip);
					wasEdited = true;
				}
			}

			return wasEdited;
		}

		static bool BSEffectShaderMaterialEditor(void* object, void* context)
		{
			auto& effectShaderMaterial = *static_cast<RE::BSEffectShaderMaterial*>(object);

			bool wasEdited = false;

			if (ImGui::DragFloat2("Falloff Angle Range", &effectShaderMaterial.falloffStartAngle, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat2("Falloff Opacity Range", &effectShaderMaterial.falloffStartOpacity,
					0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::ColorEdit4("Base Color", &effectShaderMaterial.baseColor.red))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Soft Falloff Depth", &effectShaderMaterial.softFalloffDepth, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Base Color Scale", &effectShaderMaterial.softFalloffDepth,
					0.1f))
			{
				wasEdited = true;
			}
			if (NifTexturePathEdit("Source Texture", "Source Texture",
					effectShaderMaterial.sourceTexturePath))
			{
				RE::NiSourceTexture::Load(effectShaderMaterial.sourceTexturePath, true,
					effectShaderMaterial.sourceTexture, false);
				wasEdited = true;
			}
			if (NifTexturePathEdit("Greyscale Texture", "Greyscale Texture",
					effectShaderMaterial.greyscaleTexturePath))
			{
				RE::NiSourceTexture::Load(effectShaderMaterial.greyscaleTexturePath, true,
					effectShaderMaterial.greyscaleTexture, false);
				wasEdited = true;
			}
			if (EnumSelector("Texture Clamp Mode", effectShaderMaterial.effectClampMode))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool BSPSysLODModifierEditor(void* object, void* context)
		{
			auto& lodModifier = *static_cast<RE::BSPSysLODModifier*>(object);

			bool wasEdited = false;

			if (ImGui::DragFloat2("LOD Distance Range", &lodModifier.lodBeginDistance,
					0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("End Emit Scale", &lodModifier.endEmitScale,
					0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Soft Falloff Depth", &lodModifier.endSize,
					0.1f))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool BSPSysSimpleColorModifierEditor(void* object, void* context)
		{
			auto& simpleColorModifier = *static_cast<RE::BSPSysSimpleColorModifier*>(object);

			bool wasEdited = false;

			for (size_t colorIndex = 0; colorIndex < 3; ++colorIndex)
			{
				if (ImGui::ColorEdit4(std::format("Colors[{0}]", colorIndex).c_str(), &simpleColorModifier.colors[colorIndex].red))
				{
					wasEdited = true;
				}
			}
			if (ImGui::DragFloat("Fade In Percent", &simpleColorModifier.fadeInPercent, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Fade Out Percent", &simpleColorModifier.fadeOutPercent, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat4("Color Percents", &simpleColorModifier.color1EndPercent, 0.1f))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiPSysFieldModifierEditor(void* object, void* context)
		{
			auto& fieldModifier = *static_cast<RE::NiPSysFieldModifier*>(object);

			bool wasEdited = false;

			if (NiObjectNETTargetSelector<RE::NiAVObject>("Field Object",
					*reinterpret_cast<NiObjectContext*>(context), fieldModifier.fieldObject))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Magnitude", &fieldModifier.magnitude, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Attenuation", &fieldModifier.attenuation, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::Checkbox("Use Max Distance", &fieldModifier.useMaxDistance))
			{
				wasEdited = true;
			}
			if (fieldModifier.useMaxDistance)
			{
				float maxDistance = fieldModifier.maxDistance;
				if (ImGui::DragFloat("Max Distance", &maxDistance, 0.1f))
				{
					fieldModifier.SetMaxDistance(maxDistance);
					wasEdited = true;
				}
			}

			return wasEdited;
		}

		static bool NiPSysModifierCtlrEditor(void* object, void* context)
		{
			auto& modifierCtlr = *static_cast<RE::NiPSysModifierCtlr*>(object);

			bool wasEdited = false;

			if (NiPSysModifierTargetSelector<RE::NiPSysModifier>("Modifier",
					*reinterpret_cast<NiObjectContext*>(context), modifierCtlr.targetModifier,
					[&](RE::NiPSysModifier* modifier)
					{ return modifierCtlr.IsValidInterpolatorTarget(modifier); }))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiPSysTurbulenceFieldModifierEditor(void* object, void* context)
		{
			auto& turbulenceFieldModifier = *static_cast<RE::NiPSysTurbulenceFieldModifier*>(object);

			bool wasEdited = false;

			{
				float frequency = turbulenceFieldModifier.frequency;
				if (ImGui::DragFloat("Frequency", &frequency, 0.1f))
				{
					turbulenceFieldModifier.SetFrequency(frequency);
					wasEdited = true;
				}
			}

			return wasEdited;
		}

		static bool NiPSysVortexFieldModifierEditor(void* object, void* context)
		{
			auto& vortexFieldModifier =
				*static_cast<RE::NiPSysVortexFieldModifier*>(object);

			bool wasEdited = false;
			if (NiPoint3Editor("Direction", vortexFieldModifier.direction, 0.1f))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiPSysRadialFieldModifierEditor(void* object, void* context)
		{
			auto& radialFieldModifier = *static_cast<RE::NiPSysRadialFieldModifier*>(object);

			bool wasEdited = false;
			if (ImGui::DragFloat("Radial Type", &radialFieldModifier.radialType, 0.1f))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiPSysGrowFadeModifierEditor(void* object, void* context)
		{
			auto& growFadeModifier = *static_cast<RE::NiPSysGrowFadeModifier*>(object);

			bool wasEdited = false;
			if (ImGui::DragFloat("Grow Time", &growFadeModifier.growTime, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragScalar("Grow Generation", ImGuiDataType_U16, &growFadeModifier.growGeneration))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Fade Time", &growFadeModifier.fadeTime, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragScalar("Fade Generation", ImGuiDataType_U16,
					&growFadeModifier.fadeGeneration))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Base Scale", &growFadeModifier.baseScale, 0.1f))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiPSysDragFieldModifierEditor(void* object, void* context)
		{
			auto& dragFieldModifier = *static_cast<RE::NiPSysDragFieldModifier*>(object);

			bool wasEdited = false;
			if (ImGui::Checkbox("Use Direction", &dragFieldModifier.useDirection))
			{
				wasEdited = true;
			}
			if (dragFieldModifier.useDirection)
			{
				if (NiPoint3Editor("Direction", dragFieldModifier.direction, 0.1f))
				{
					wasEdited = true;
				}
			}

			return wasEdited;
		}

		static bool NiPSysAirFieldModifierEditor(void* object, void* context)
		{
			auto& airFieldModifier = *static_cast<RE::NiPSysAirFieldModifier*>(object);

			bool wasEdited = false;
			{
				RE::NiPoint3 direction = airFieldModifier.direction;
				if (NiPoint3Editor("Direction", direction, 0.1f))
				{
					airFieldModifier.SetDirection(direction);
					wasEdited = true;
				}
			}
			if (ImGui::DragFloat("Air Friction", &airFieldModifier.airFriction, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Inherit Velocity", &airFieldModifier.inheritVelocity, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::Checkbox("Inherit Rotation", &airFieldModifier.inheritRotation))
			{
				wasEdited = true;
			}
			if (ImGui::Checkbox("Component Only", &airFieldModifier.componentOnly))
			{
				wasEdited = true;
			}
			if (ImGui::Checkbox("Enable Spread", &airFieldModifier.enableSpread))
			{
				wasEdited = true;
			}
			if (airFieldModifier.enableSpread)
			{
				if (ImGui::DragFloat("Spread", &airFieldModifier.spread, 0.1f))
				{
					wasEdited = true;
				}
			}

			return wasEdited;
		}

		static bool NiPSysColliderManagerEditor(void* object, void* context)
		{
			auto& colliderManager = *static_cast<RE::NiPSysColliderManager*>(object);

			bool wasEdited = false;

			auto& rttiCache = RTTICache::Instance();

			{
				size_t colliderIndex = 0;
				RE::NiPSysCollider* currentCollider = colliderManager.collider.get();
				RE::NiPSysCollider* previousCollider = nullptr;
				while (currentCollider != nullptr)
				{
					const std::string& typeName = rttiCache.GetTypeName(currentCollider);
					if (PushingCollapsingHeader(std::format("[Collider] <{}>##{}", typeName, colliderIndex).c_str()))
					{
						if (ImGui::Button("Remove"))
						{
							(previousCollider == nullptr ? colliderManager.collider :
														   previousCollider->nextCollider) =
								currentCollider->nextCollider;
							wasEdited = true;
						}
						else
						{
							if (rttiCache.BuildEditor(colliderManager.collider.get(), context))
							{
								wasEdited = true;
							}
						}
						ImGui::TreePop();
					}

					previousCollider = currentCollider;
					currentCollider = previousCollider->nextCollider.get();
					++colliderIndex;
				}
			}

			{
				static const auto colliderTypeDescriptor =
					&*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysCollider);
				static const RTTI* newColliderRtti = nullptr;

				NiObjectTypeSelector("##NewCollider", *colliderTypeDescriptor, newColliderRtti);
				ImGui::SameLine();
				if (ImGui::Button("Add Collider"))
				{
					if (newColliderRtti != nullptr)
					{
						if (auto newCollider =
								static_cast<RE::NiPSysCollider*>(newColliderRtti->constructor()))
						{
							newCollider->parent = &colliderManager;

							RE::NiPSysCollider* currentCollider = colliderManager.collider.get();
							if (currentCollider == nullptr)
							{
								colliderManager.collider = RE::NiPointer<RE::NiPSysCollider>(newCollider);
							}
							else
							{
								while (currentCollider->nextCollider != nullptr)
								{
									currentCollider = currentCollider->nextCollider.get();
								}
								currentCollider->nextCollider = RE::NiPointer<RE::NiPSysCollider>(newCollider);
							}
						}
					}
				}
			}

			return wasEdited;
		}

		static bool NiPSysGravityFieldModifierEditor(void* object, void* context)
		{
			auto& gravityFieldModifier = *static_cast<RE::NiPSysGravityFieldModifier*>(object);

			bool wasEdited = false;
			{
				RE::NiPoint3 direction = gravityFieldModifier.direction;
				if (NiPoint3Editor("Direction", direction, 0.1f))
				{
					gravityFieldModifier.SetDirection(direction);
					wasEdited = true;
				}
			}

			return wasEdited;
		}

		static bool BSPSysStripUpdateModifierEditor(void* object, void* context)
		{
			auto& stripUpdateModifier = *static_cast<RE::BSPSysStripUpdateModifier*>(object);

			bool wasEdited = false;
			if (ImGui::DragFloat("Update Delta Time", &stripUpdateModifier.updateDeltaTime, 0.1f))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool BSPSysSubTexModifierEditor(void* object, void* context)
		{
			auto& subTexModifier = *static_cast<RE::BSPSysSubTexModifier*>(object);

			bool wasEdited = false;
			if (ImGui::DragFloat("Start Frame", &subTexModifier.startFrame, 1.f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Start Frame Fudge", &subTexModifier.startFrameFudge, 1.f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("End Frame", &subTexModifier.endFrame, 1.f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Loop Start Frame", &subTexModifier.loopStartFrame, 1.f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Loop Start Frame Fudge", &subTexModifier.loopStartFrameFudge, 1.f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Frame Count", &subTexModifier.frameCount, 1.f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Frame Count Fudge", &subTexModifier.frameCountFudge, 1.f))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiPSysColliderEditor(void* object, void* context)
		{
			auto& collider = *static_cast<RE::NiPSysCollider*>(object);

			bool wasEdited = false;
			if (ImGui::DragFloat("Bounce", &collider.bounce, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::Checkbox("Spawn On Collider", &collider.spawnOnCollide))
			{
				wasEdited = true;
			}
			if (collider.spawnOnCollide)
			{
				if (NiPSysModifierTargetSelector<RE::NiPSysSpawnModifier>("Spawn Modifier",
						*reinterpret_cast<NiObjectContext*>(context), collider.spawnModifier))
				{
					wasEdited = true;
				}
			}
			if (ImGui::Checkbox("Die On Collider", &collider.dieOnCollide))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiPSysSphericalColliderEditor(void* object, void* context)
		{
			auto& sphericalCollider = *static_cast<RE::NiPSysSphericalCollider*>(object);

			bool wasEdited = false;
			if (ImGui::DragFloat("Radius", &sphericalCollider.radius, 0.1f))
			{
				wasEdited = true;
			}

			return wasEdited;
		}

		static bool NiPSysPlanarColliderEditor(void* object, void* context)
		{
			auto& planarCollider = *static_cast<RE::NiPSysPlanarCollider*>(object);

			bool wasEdited = false;
			if (ImGui::DragFloat("Width", &planarCollider.width, 0.1f))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Height", &planarCollider.height, 0.1f))
			{
				wasEdited = true;
			}
			if (NiPoint3Editor("X Axis", planarCollider.xAxis, 0.1f))
			{
				wasEdited = true;
			}
			if (NiPoint3Editor("Y Axis", planarCollider.yAxis, 0.1f))
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
			NiObjectContext rootContext(object);
			wasEdited = rttiCache.BuildEditor(&object, &rootContext);
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
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiBillboardNode),
			SNiObjectEditor::NiBillboardNodeEditor);

		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSGeometry),
			SNiObjectEditor::BSGeometryEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSTriShape),
			SNiObjectEditor::BSTriShapeEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiParticles),
			SNiObjectEditor::NiParticlesEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiParticleSystem),
			SNiObjectEditor::NiParticleSystemEditor);

		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiAlphaProperty),
			SNiObjectEditor::NiAlphaPropertyEditor);
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
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSEffectShaderMaterial),
			SNiObjectEditor::BSEffectShaderMaterialEditor);

		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSXFlags),
			SNiObjectEditor::BSXFlagsEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiIntegerExtraData),
			SNiObjectEditor::NiIntegerExtraDataEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiStringExtraData),
			SNiObjectEditor::NiStringExtraDataEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSFurnitureMarkerNode),
			SNiObjectEditor::BSFurnitureMarkerNodeEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSInvMarker),
			SNiObjectEditor::BSInvMarkerEditor);

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

		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiTimeController),
			SNiObjectEditor::NiTimeControllerEditor);
		rttiCache.RegisterEditor(
			*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiSingleInterpController),
			SNiObjectEditor::NiSingleInterpControllerEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiControllerManager),
			SNiObjectEditor::NiControllerManagerEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSLagBoneController),
			SNiObjectEditor::BSLagBoneControllerEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysModifierCtlr),
			SNiObjectEditor::NiPSysModifierCtlrEditor);

		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSPSysLODModifier),
			SNiObjectEditor::BSPSysLODModifierEditor);
		rttiCache.RegisterEditor(
			*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSPSysSimpleColorModifier),
			SNiObjectEditor::BSPSysSimpleColorModifierEditor);
		rttiCache.RegisterEditor(
			*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSPSysStripUpdateModifier),
			SNiObjectEditor::BSPSysStripUpdateModifierEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_BSPSysSubTexModifier),
			SNiObjectEditor::BSPSysSubTexModifierEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysAgeDeathModifier),
			SNiObjectEditor::NiPSysAgeDeathModifierEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysAirFieldModifier),
			SNiObjectEditor::NiPSysAirFieldModifierEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysBombModifier),
			SNiObjectEditor::NiPSysBombModifierEditor);
		rttiCache.RegisterEditor(
			*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysBoundUpdateModifier),
			SNiObjectEditor::NiPSysBoundUpdateModifierEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysBoxEmitter),
			SNiObjectEditor::NiPSysBoxEmitterEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysColliderManager),
			SNiObjectEditor::NiPSysColliderManagerEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysCylinderEmitter),
			SNiObjectEditor::NiPSysCylinderEmitterEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysDragModifier),
			SNiObjectEditor::NiPSysDragModifierEditor);
		rttiCache.RegisterEditor(
			*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysDragFieldModifier),
			SNiObjectEditor::NiPSysDragFieldModifierEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysEmitter),
			SNiObjectEditor::NiPSysEmitterEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysFieldModifier),
			SNiObjectEditor::NiPSysFieldModifierEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysGravityModifier),
			SNiObjectEditor::NiPSysGravityModifierEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysGravityFieldModifier),
			SNiObjectEditor::NiPSysGravityFieldModifierEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysGrowFadeModifier),
			SNiObjectEditor::NiPSysGrowFadeModifierEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysMeshEmitter),
			SNiObjectEditor::NiPSysMeshEmitterEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysModifier),
			SNiObjectEditor::NiPSysModifierEditor);
		rttiCache.RegisterEditor(
			*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysRadialFieldModifier),
			SNiObjectEditor::NiPSysRadialFieldModifierEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysRotationModifier),
			SNiObjectEditor::NiPSysRotationModifierEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysSpawnModifier),
			SNiObjectEditor::NiPSysSpawnModifierEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysSphereEmitter),
			SNiObjectEditor::NiPSysSphereEmitterEditor);
		rttiCache.RegisterEditor(
			*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysTurbulenceFieldModifier),
			SNiObjectEditor::NiPSysTurbulenceFieldModifierEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysVolumeEmitter),
			SNiObjectEditor::NiPSysVolumeEmitterEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysVortexFieldModifier),
			SNiObjectEditor::NiPSysVortexFieldModifierEditor);

		rttiCache.RegisterEditor(
			*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysCollider),
			SNiObjectEditor::NiPSysColliderEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysSphericalCollider),
			SNiObjectEditor::NiPSysSphericalColliderEditor);
		rttiCache.RegisterEditor(*REL::Relocation<TypeDescriptor*>(RE::RTTI_NiPSysPlanarCollider),
			SNiObjectEditor::NiPSysPlanarColliderEditor);
	}
}
